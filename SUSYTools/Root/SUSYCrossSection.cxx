/*
  Copyright (C) 2002-2018 CERN for the benefit of the ATLAS collaboration
*/

// Class header
#include "SUSYTools/SUSYCrossSection.h"

// Find the xsec files in datapath
#include "PathResolver/PathResolver.h"

// For directory commands
#include <dirent.h>

// Error messages
#include <iostream>

// Text file i/o
#include <fstream>

SUSY::CrossSectionDB::CrossSectionDB(const std::string& txtfilenameOrDir, bool usePathResolver, bool isExtended, bool usePMGTool)
  : m_pmgxs("")
{
  setExtended(isExtended);
  setUsePMGTool(usePMGTool);

  // configuring the PMG tool... 
  m_pmgxs.setTypeAndName("PMGTools::PMGCrossSectionTool/PMGCrossSectionTool");
  m_pmgxs.retrieve().ignore(); // Ignore the status code
  std::vector<std::string> inFiles={};
  inFiles.push_back( PathResolverFindCalibFile("dev/PMGTools/PMGxsecDB_mc16.txt") );
  m_pmgxs->readInfosFromFiles( inFiles );

  if (usePathResolver) {
    std::string fullPath = PathResolverFindCalibDirectory(txtfilenameOrDir);
    
    DIR* dp = opendir(fullPath.c_str());
    if (dp) {
      struct dirent * de;
      while ((de = readdir(dp)) != NULL) {
        loadFile(fullPath + de->d_name);
      }
      if (closedir(dp)!=0){
        std::cerr << "CrossSectionDB::CrossSectionDB ERROR Problem closing directory" << std::endl;
      }
    }
    else {
      std::string fullPathToFile = PathResolverFindCalibFile(txtfilenameOrDir);
      loadFile(fullPathToFile.c_str());
    }
  }
  else {
    DIR* dp = opendir(txtfilenameOrDir.c_str());
    if (dp) {
      struct dirent * de;
      while ((de = readdir(dp)) != NULL) {
        loadFile(txtfilenameOrDir + de->d_name);
      }
      if (closedir(dp)!=0){
        std::cerr << "CrossSectionDB::CrossSectionDB ERROR Problem closing directory" << std::endl;
      }
    } else {
      loadFile(txtfilenameOrDir.c_str());
    }
  }
}

void SUSY::CrossSectionDB::loadFile(const std::string& txtfilename){

  std::string line;
  
  std::ifstream in(txtfilename.c_str());
  if (!in) return;
  while ( getline(in, line) ){
    // skip leading blanks (in case there are some in front of a comment)
    if ( !line.empty() ){
      while ( line[0] == ' ' ) line.erase(0, 1);
    }
    // skip lines that do not start with a number, they are comments
    if ( !line.empty() && isdigit(line[0]) ){
      std::stringstream is(line);
      int id;
      std::string name;
      float xsect, kfactor, efficiency, relunc;
      float sumweight = -1, stat = -1;
      is >> id >> name >> xsect >> kfactor >> efficiency >> relunc;
      if (m_extended == true){
          is >> sumweight >> stat;
      }
      m_xsectDB[Key(id, name)] = Process(id, name, xsect, kfactor, efficiency, relunc, sumweight, stat);
    }
  }
}

// Convenient accessor for finding based on *only* a process ID
SUSY::CrossSectionDB::xsDB_t::iterator SUSY::CrossSectionDB::my_find( const int proc ) {
  for (SUSY::CrossSectionDB::xsDB_t::iterator it = m_xsectDB.begin(); it!=m_xsectDB.end();it++){
    if (it->second.ID()==proc) return it;
  }
  return m_xsectDB.end();
}

// Extend the record based on information from a second file
void SUSY::CrossSectionDB::extend(const std::string& txtfilename){
  // Just like the above function, but with more functionality
  std::string line;

  std::ifstream in(txtfilename.c_str());
  if (!in) return;
  while ( getline(in, line) )
    {
      // skip leading blanks (in case there are some in front of a comment)
      if ( !line.empty() ){
          while ( line[0] == ' ' ) line.erase(0, 1);
      }
      // skip lines that do not start with a number, they are comments
      if ( !line.empty() && isdigit(line[0]) ){
          std::stringstream is(line);
          int id;
          std::string name;
          float xsect, kfactor, efficiency, relunc;
          float sumweight = -1, stat = -1;
          is >> id;
          auto my_it = my_find( id );
          if (my_it==m_xsectDB.end()){
            is >> name >> xsect >> kfactor >> efficiency >> relunc;
            if (m_extended == true){
              is >> sumweight >> stat;
            }
            m_xsectDB[Key(id, name)] = Process(id, name, xsect, kfactor, efficiency, relunc, sumweight, stat);
          } else {
            // Now we have extended records
            if (!m_extended) m_extended=true;
            is >> sumweight >> stat;
            my_it->second.sumweight(sumweight);
            my_it->second.stat(stat);
          }
        }
    }

}

SUSY::CrossSectionDB::Process SUSY::CrossSectionDB::process(int id, int proc) const
{
  // for background x-sections, use the PMG tool
  if(proc==0 && m_usePMGTool) {
    return Process( id, m_pmgxs->getSampleName(id), m_pmgxs->getAMIXsection(id), m_pmgxs->getKfactor(id), m_pmgxs->getFilterEff(id), -1, -1, -1 );  
  } else {
    const Key k(id, proc);
    xsDB_t::const_iterator pos = m_cache.find(k);
    if (pos != m_cache.end()) {
      return pos->second;
    } else {
      pos = m_xsectDB.find(k);
      if (pos != m_xsectDB.end()) {
	return pos->second;
      }
    }
  }
  return Process();
}


unsigned int SUSY::finalState(const int SUSY_Spart1_pdgId, const int SUSY_Spart2_pdgId)
{
  int ngluino = 0;
  int nsquark = 0; // (up and down type without bottom/top)
  int nantisquark = 0; // (up and down type without bottom/top)

  int nsbottom = 0;
  int nstop = 0;
  int nsbottom2 = 0;
  int nstop2 = 0;
  int nantisbottom = 0;
  int nantistop = 0;
  int nantisbottom2 = 0;
  int nantistop2 = 0;

  int nchi01 = 0;
  int nchi02 = 0;
  int nchi03 = 0;
  int nchi04 = 0;
  int nch1plus = 0;
  int nch2plus = 0;
  int nch1minus = 0;
  int nch2minus = 0;

  // sleptons
  int nsmuonRplus = 0;
  int nsmuonRminus = 0;
  int nselecRplus = 0;
  int nselecRminus = 0;

  int nsmuonLplus = 0;
  int nsmuonLminus = 0;
  int nselecLplus = 0;
  int nselecLminus = 0;

  int nstau1plus = 0;
  int nstau1minus = 0;
  int nstau2plus = 0;
  int nstau2minus = 0;

  // snutrinos
  int nselnuL = 0;
  int nsmunuL = 0;
  int nstaunuL = 0;

  int nother = 0;

  //Classification of the event follows (gg, sq...):

  if      (abs(SUSY_Spart1_pdgId) == 1000022) nchi01++;
  else if (abs(SUSY_Spart1_pdgId) == 1000023) nchi02++;
  else if (abs(SUSY_Spart1_pdgId) == 1000025) nchi03++;
  else if (abs(SUSY_Spart1_pdgId) == 1000035) nchi04++;
  else if (    SUSY_Spart1_pdgId == 1000024) nch1plus++;
  else if (    SUSY_Spart1_pdgId == -1000024) nch1minus++;
  else if (    SUSY_Spart1_pdgId == 1000037) nch2plus++;
  else if (    SUSY_Spart1_pdgId == -1000037) nch2minus++;
  else if (    SUSY_Spart1_pdgId == 1000021) ngluino++;
  else if ((abs(SUSY_Spart1_pdgId) > 1000000 && abs(SUSY_Spart1_pdgId) <= 1000004) || (abs(SUSY_Spart1_pdgId) > 2000000 && abs(SUSY_Spart1_pdgId) <= 2000004)) {
    if (SUSY_Spart1_pdgId > 0) nsquark++;
    else nantisquark++;
  }
  else if (SUSY_Spart1_pdgId == 1000005) nsbottom++;
  else if (SUSY_Spart1_pdgId == 1000006) nstop++;
  else if (SUSY_Spart1_pdgId == 2000005) nsbottom2++;
  else if (SUSY_Spart1_pdgId == 2000006) nstop2++;
  else if (SUSY_Spart1_pdgId == -1000005) nantisbottom++;
  else if (SUSY_Spart1_pdgId == -1000006) nantistop++;
  else if (SUSY_Spart1_pdgId == -2000005) nantisbottom2++;
  else if (SUSY_Spart1_pdgId == -2000006) nantistop2++;
  else if (SUSY_Spart1_pdgId == 2000011) nselecRminus++;
  else if (SUSY_Spart1_pdgId == -2000011) nselecRplus++;
  else if (SUSY_Spart1_pdgId == 1000011) nselecLminus++;
  else if (SUSY_Spart1_pdgId == -1000011) nselecLplus++;
  else if (abs(SUSY_Spart1_pdgId) == 1000012) nselnuL++;
  else if (SUSY_Spart1_pdgId == 2000013) nsmuonRminus++;
  else if (SUSY_Spart1_pdgId == -2000013) nsmuonRplus++;
  else if (SUSY_Spart1_pdgId == 1000013) nsmuonLminus++;
  else if (SUSY_Spart1_pdgId == -1000013) nsmuonLplus++;
  else if (abs(SUSY_Spart1_pdgId) == 1000014) nsmunuL++;
  else if (SUSY_Spart1_pdgId == 1000015) nstau1minus++;
  else if (SUSY_Spart1_pdgId == -1000015) nstau1plus++;
  else if (SUSY_Spart1_pdgId == 2000015) nstau2minus++;
  else if (SUSY_Spart1_pdgId == -2000015) nstau2plus++;
  else if (abs(SUSY_Spart1_pdgId) == 1000016) nstaunuL++;
  else nother++;




  if (abs(SUSY_Spart2_pdgId) == 1000022) nchi01++;
  else if (abs(SUSY_Spart2_pdgId) == 1000023) nchi02++;
  else if (abs(SUSY_Spart2_pdgId) == 1000025) nchi03++;
  else if (abs(SUSY_Spart2_pdgId) == 1000035) nchi04++;
  else if (SUSY_Spart2_pdgId == 1000024) nch1plus++;
  else if (SUSY_Spart2_pdgId == -1000024) nch1minus++;
  else if (SUSY_Spart2_pdgId == 1000037) nch2plus++;
  else if (SUSY_Spart2_pdgId == -1000037) nch2minus++;

  else if (SUSY_Spart2_pdgId == 1000021) ngluino++;
  else if ((abs(SUSY_Spart2_pdgId) > 1000000 && abs(SUSY_Spart2_pdgId) <= 1000004) || (abs(SUSY_Spart2_pdgId) > 2000000 && abs(SUSY_Spart2_pdgId) <= 2000004)) {
    if (SUSY_Spart2_pdgId > 0) nsquark++;
    else nantisquark++;
  }
  else if (SUSY_Spart2_pdgId == 1000005) nsbottom++;
  else if (SUSY_Spart2_pdgId == 1000006) nstop++;
  else if (SUSY_Spart2_pdgId == 2000005) nsbottom2++;
  else if (SUSY_Spart2_pdgId == 2000006) nstop2++;
  else if (SUSY_Spart2_pdgId == -1000005) nantisbottom++;
  else if (SUSY_Spart2_pdgId == -1000006) nantistop++;
  else if (SUSY_Spart2_pdgId == -2000005) nantisbottom2++;
  else if (SUSY_Spart2_pdgId == -2000006) nantistop2++;

  else if (SUSY_Spart2_pdgId == 2000011) nselecRminus++;
  else if (SUSY_Spart2_pdgId == -2000011) nselecRplus++;
  else if (SUSY_Spart2_pdgId == 1000011) nselecLminus++;
  else if (SUSY_Spart2_pdgId == -1000011) nselecLplus++;
  else if (abs(SUSY_Spart2_pdgId) == 1000012) nselnuL++;
  else if (SUSY_Spart2_pdgId == 2000013) nsmuonRminus++;
  else if (SUSY_Spart2_pdgId == -2000013) nsmuonRplus++;
  else if (SUSY_Spart2_pdgId == 1000013) nsmuonLminus++;
  else if (SUSY_Spart2_pdgId == -1000013) nsmuonLplus++;
  else if (abs(SUSY_Spart2_pdgId) == 1000014) nsmunuL++;
  else if (SUSY_Spart2_pdgId == 1000015) nstau1minus++;
  else if (SUSY_Spart2_pdgId == -1000015) nstau1plus++;
  else if (SUSY_Spart2_pdgId == 2000015) nstau2minus++;
  else if (SUSY_Spart2_pdgId == -2000015) nstau2plus++;
  else if (abs(SUSY_Spart2_pdgId) == 1000016) nstaunuL++;
  else nother++;


  ///Final classification
  // gluino/squark + X
  if (ngluino == 1 && (nsquark == 1 || nantisquark == 1)) return 1;
  else if (ngluino == 2) return 2;
  else if (nsquark == 2 || nantisquark == 2) return 3;
  else if (nsquark == 1 && nantisquark == 1) return 4;

  else if (nsbottom == 1 && nantisbottom == 1) return 51;
  else if (nsbottom2 == 1 && nantisbottom2 == 1) return 52;
  else if (nstop == 1 && nantistop == 1) return 61;
  else if (nstop2 == 1 && nantistop2 == 1) return 62;

  else if (ngluino == 1 && nchi01 == 1) return 71;
  else if (ngluino == 1 && nchi02 == 1) return 72;
  else if (ngluino == 1 && nchi03 == 1) return 73;
  else if (ngluino == 1 && nchi04 == 1) return 74;

  else if (ngluino == 1 && nch1plus == 1) return 75;
  else if (ngluino == 1 && nch2plus == 1) return 76;
  else if (ngluino == 1 && nch1minus == 1) return 77;
  else if (ngluino == 1 && nch2minus == 1) return 78;

  else if ((nsquark == 1 || nantisquark == 1) && nchi01 == 1) return 81;
  else if ((nsquark == 1 || nantisquark == 1) && nchi02 == 1) return 82;
  else if ((nsquark == 1 || nantisquark == 1) && nchi03 == 1) return 83;
  else if ((nsquark == 1 || nantisquark == 1) && nchi04 == 1) return 84;

  else if ((nsquark == 1 || nantisquark == 1) && nch1plus == 1) return 85;
  else if ((nsquark == 1 || nantisquark == 1) && nch2plus == 1) return 86;
  else if ((nsquark == 1 || nantisquark == 1) && nch1minus == 1) return 87;
  else if ((nsquark == 1 || nantisquark == 1) && nch2minus == 1) return 88;


  // Gaugino pair-production
  // chi^{0}_1 + X
  else if (nchi01 == 2) return 111;
  else if (nchi01 == 1 && nchi02 == 1) return 112;
  else if (nchi01 == 1 && nchi03 == 1) return 113;
  else if (nchi01 == 1 && nchi04 == 1) return 114;
  else if (nchi01 == 1 && nch1plus == 1) return 115;
  else if (nchi01 == 1 && nch2plus == 1) return 116;
  else if (nchi01 == 1 && nch1minus == 1) return 117;
  else if (nchi01 == 1 && nch2minus == 1) return 118;

  // chi^{0}_2 + X
  else if (nchi02 == 2) return 122;
  else if (nchi02 == 1 && nchi03 == 1) return 123;
  else if (nchi02 == 1 && nchi04 == 1) return 124;
  else if (nchi02 == 1 && nch1plus == 1) return 125;
  else if (nchi02 == 1 && nch2plus == 1) return 126;
  else if (nchi02 == 1 && nch1minus == 1) return 127;
  else if (nchi02 == 1 && nch2minus == 1) return 128;

  // chi^{0}_3 + X
  else if (nchi03 == 2) return 133;
  else if (nchi03 == 1 && nchi04 == 1) return 134;
  else if (nchi03 == 1 && nch1plus == 1) return 135;
  else if (nchi03 == 1 && nch2plus == 1) return 136;
  else if (nchi03 == 1 && nch1minus == 1) return 137;
  else if (nchi03 == 1 && nch2minus == 1) return 138;

  // chi^{0}_4 + X
  else if (nchi04 == 2) return 144;
  else if (nchi04 == 1 && nch1plus == 1) return 145;
  else if (nchi04 == 1 && nch2plus == 1) return 146;
  else if (nchi04 == 1 && nch1minus == 1) return 147;
  else if (nchi04 == 1 && nch2minus == 1) return 148;

  // chi^{+}_1/2 + chi^{-}_1/2
  else if (nch1plus == 1 && nch1minus == 1) return 157;
  else if (nch1plus == 1 && nch2minus == 1) return 158;

  else if (nch2plus == 1 && nch1minus == 1) return 167;
  else if (nch2plus == 1 && nch2minus == 1) return 168;

  // slepton
  else if (nselecLplus == 1 && nselecLminus == 1) return 201; // sElectronLPair
  else if (nselecRplus == 1 && nselecRminus == 1) return 202; // sElectronRPair
  else if (nselnuL == 2) return 203; // sElectron neutrino pair
  else if (nselecLplus == 1 && nselnuL == 1) return 204; // sElectron+ sNutrino
  else if (nselecLminus == 1 && nselnuL == 1) return 205; // sElectron- sNutrino
  else if (nstau1plus == 1 && nstau1minus == 1) return 206;
  else if (nstau2plus == 1 && nstau2minus == 1) return 207;
  else if ((nstau1plus == 1 || nstau1minus == 1) && (nstau2plus == 1 || nstau2minus == 1)) return 208;
  else if (nstaunuL == 2) return 209; // sTau neutrino pair
  else if (nstau1plus == 1 && nstaunuL == 1) return 210;
  else if (nstau1minus == 1 && nstaunuL == 1) return 211;
  else if (nstau2plus == 1 && nstaunuL == 1) return 212;
  else if (nstau2minus == 1 && nstaunuL == 1) return 213;

  else if (nsmuonLplus == 1 && nsmuonLminus == 1) return 216; // sMuonPair
  else if (nsmuonRplus == 1 && nsmuonRminus == 1) return 217; // sMuonPair
  else if (nsmunuL == 2) return 218; // sMuon neutrino pair
  else if (nsmuonLplus == 1 && nsmunuL == 1) return 219; // sMuon+ sNutrino
  else if (nsmuonLminus == 1 && nsmunuL == 1) return 220; // sMuon- sNutrino

  std::cerr << "ERROR. could not determine finalState for:" << std::endl;
  std::cerr << "  SUSY_Spart1_pdgId: " << SUSY_Spart1_pdgId << std::endl;
  std::cerr << "  SUSY_Spart2_pdgId: " << SUSY_Spart2_pdgId << std::endl;
  std::cerr << "Returning 0" << std::endl;

  return 0;
}
