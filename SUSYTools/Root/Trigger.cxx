/*
  Copyright (C) 2002-2018 CERN for the benefit of the ATLAS collaboration
*/

// This source file implements all of the functions related to <OBJECT>
// in the SUSYObjDef_xAOD class

// Local include(s):
#include "SUSYTools/SUSYObjDef_xAOD.h"

#include "TrigConfInterfaces/ITrigConfigTool.h"
#include "TrigDecisionTool/TrigDecisionTool.h"
#include "TriggerMatchingTool/IMatchingTool.h"
#include "TriggerAnalysisInterfaces/ITrigGlobalEfficiencyCorrectionTool.h"
#include "TrigDecisionTool/FeatureContainer.h"
#include "TrigDecisionTool/Conditions.h"

#include "xAODTrigMissingET/TrigMissingETContainer.h"

#ifndef XAOD_STANDALONE // For now metadata is Athena-only
#include "AthAnalysisBaseComps/AthAnalysisHelper.h"
#endif

#include <regex>

namespace ST {

  const static SG::AuxElement::Decorator<char> dec_trigmatched("trigmatched");

bool SUSYObjDef_xAOD::IsMETTrigPassed(unsigned int runnumber, bool j400_OR) const {

  // Returns MET trigger decision for recommended lowest unprescaled evolution described in 
  // https://twiki.cern.ch/twiki/bin/viewauth/Atlas/LowestUnprescaled
  // For period vs run number, see https://atlas-tagservices.cern.ch/tagservices/RunBrowser/runBrowserReport/rBR_Period_Report.php

  // if no runNumber specified, just read it from the current event
  unsigned int rn;
  if(runnumber>0){
    rn = runnumber;
  }
  else{
    rn = GetRunNumber(); // it takes care of dealing with data and MC
  }
  
  int year = treatAsYear(rn);

  if (year == 2015)                                      return IsMETTrigPassed("HLT_xe70_mht",j400_OR); //2015
  else if(year == 2016 && rn >= 296939 && rn <= 302872 ) return IsMETTrigPassed("HLT_xe90_mht_L1XE50",j400_OR); //2016 A-D3
  else if(year == 2016 && rn >= 302919 && rn <= 303892 ) return IsMETTrigPassed("HLT_xe100_mht_L1XE50",j400_OR); //2016 D4-F1
  else if(year == 2016 && rn >= 303943)                  return IsMETTrigPassed("HLT_xe110_mht_L1XE50",j400_OR); //2016 F2-(open) 
  else if(year == 2017 && rn >= 325713 && rn <= 331975 ) return IsMETTrigPassed("HLT_xe110_pufit_L1XE55", false); // 2017 B1-D5
  else if(year == 2017 && rn >= 332304 )                 return IsMETTrigPassed("HLT_xe110_pufit_L1XE50", false); // 2017 D6-(open)
  else if(year == 2018 && rn >= 348885 && rn <= 350013 ) return IsMETTrigPassed("HLT_xe110_pufit_xe70_L1XE50", false); // 2018 B-C5
  else if(year == 2018 && rn >= 350067 )                 return IsMETTrigPassed("HLT_xe110_pufit_xe65_L1XE50", false); // 2018 C5-(open)
  return false; 
}

// Can't be const because of the lazy init of the map - JBurr: Now fixed
bool SUSYObjDef_xAOD::IsMETTrigPassed(const std::string& triggerName, bool j400_OR) const {
  // NB - this now applies to the entire function...
  //std::string L1item = "L1_XE50"; For now, I'll assume all the triggers use L1_XE50 - might need changing in the future.

  // First check if we're affected by the L1_XE50 bug
  bool L1_XE50 = m_trigDecTool->isPassed("L1_XE50");
  bool L1_XE55 = m_trigDecTool->isPassed("L1_XE55");
  bool HLT_noalg_L1J400 = m_trigDecTool->isPassed("HLT_noalg_L1J400");
  if (!L1_XE50 && j400_OR && HLT_noalg_L1J400) {
    return m_emulateHLT(triggerName);
  }
  else if (L1_XE50 || L1_XE55) {
    // See if the TDT knows about this
    if (m_isTrigInTDT(triggerName) ) return m_trigDecTool->isPassed(triggerName);
    else return m_emulateHLT(triggerName);
  }
  return false;
}

bool SUSYObjDef_xAOD::m_isTrigInTDT(const std::string& triggerName) const {
  auto mapItr = m_checkedTriggers.find(triggerName);
  if ( mapItr == m_checkedTriggers.end() ) {
    auto cg = m_trigDecTool->getChainGroup(triggerName);
    return m_checkedTriggers[triggerName] = cg->getListOfTriggers().size() != 0;
  }
  else {
    return mapItr->second;
  }
}


bool SUSYObjDef_xAOD::m_emulateHLT(const std::string& triggerName) const {
  // First, check if we've already tried using this trigger
  auto funcItr = m_metTriggerFuncs.find(triggerName);
  if (funcItr != m_metTriggerFuncs.end() )
    return funcItr->second();

  // Next, check that it really is a HLT MET trigger
  if (triggerName.substr(0,6) != "HLT_xe") {
    ATH_MSG_ERROR( "Requested trigger " << triggerName << " isn't a MET trigger! (HLT MET items should begin with 'HLT_xe'). Will return false." );
    return false;
  }

  // Next, parse the name to work out which containers are needed to emulate the decision
  std::vector<std::pair<int, std::string> > hypos; // algorithms and thresholds needed
  std::string temp(triggerName);
  // Note, we need to split on '_AND_' used for the combined mht+cell trigger
  do {
    auto pos = temp.find("_AND_");
    std::string itemName = temp.substr(0, pos);
    ATH_MSG_DEBUG( "Split trigger item " << itemName );
    if (pos == std::string::npos)
      temp = "";
    else
      temp = temp.substr(pos + 5);

    std::regex expr("HLT_xe([[:digit:]]+)_?(mht|pufit|pueta|tc_lcw|)_?(?:L1XE([[:digit:]]+)|)");
    std::smatch sm;
    if (!std::regex_match(itemName, sm, expr) ) {
      ATH_MSG_WARNING( "Regex reading for item " << itemName << " (" << triggerName << ") failed! Will be ignored!" );
      continue;
    }
    if (sm.size() < 4) { // first object is the whole match, then there are three capture groups
      ATH_MSG_WARNING( "Regex failed to capture the right groups for item " << itemName << " (" << triggerName << ") failed! Will be ignored!" );
      for (unsigned int ii = 0; ii < sm.size(); ++ii) {
        ATH_MSG_WARNING( sm[ii] );
      }
      continue;
    }
    int threshold = std::stoi(sm[1] );
    std::string algKey = sm[2];
    std::string metContBaseName = "HLT_xAOD__TrigMissingETContainer_TrigEFMissingET";
    if (algKey == "") hypos.push_back(std::make_pair(threshold, metContBaseName) );
    else if (algKey == "mht") hypos.push_back(std::make_pair(threshold, metContBaseName+"_mht") );
    else if (algKey == "pufit") hypos.push_back(std::make_pair(threshold, metContBaseName+"_topocl_PUC") );
    else if (algKey == "pueta") hypos.push_back(std::make_pair(threshold, metContBaseName+"_topocl_PS") );
    else if (algKey == "tc_lcw") hypos.push_back(std::make_pair(threshold, metContBaseName+"topocl") );

    ATH_MSG_DEBUG( "Container: " << hypos.back().second << ", Threshold: " << hypos.back().first );
   
    if (sm[3] != "" && sm[3] != "50") {
      ATH_MSG_WARNING( "The trigger requires a different L1 item to L1_XE50! This currently isn't allowed for in the code so the emulation will be slightly wrong" );
      // Note, now the L1 part is done in the previous section. However I'm keeping this warning here.
    }
  }
  while (!temp.empty() );

  // Check if we have the containers and construct the lambda
  // Already done the L1 decision - only care about HLT
  std::function<bool()> lambda;
  bool hasRequired = true;
  if (hypos.size() == 0) lambda = [] () {return false;};
  else { 
    for (const auto& pair : hypos) {
      if (evtStore()->contains<xAOD::TrigMissingETContainer>(pair.second) ) {
        auto lambda_hypo = [this, pair] () {
          const xAOD::TrigMissingETContainer* cont(0);
          if (evtStore()->retrieve(cont, pair.second) ) {
            if (!cont->size()) return false;
            float ex = cont->front()->ex() * 0.001;
            float ey = cont->front()->ey() * 0.001;
            float met = sqrt(ex*ex + ey*ey);
            return met > pair.first;
          }
          else {
            return false;
          }
        };
        // an empty std::function evaluates to false
        if (lambda) {
          lambda = [lambda, lambda_hypo] () {
            return lambda() && lambda_hypo();
          };
        }
        else {
          lambda = lambda_hypo;
        }
      }
      else {
        hasRequired = false;
        ATH_MSG_WARNING( "Container: " << pair.second << " missing!" );
      }
    }
  }
  if (hasRequired) {
    m_metTriggerFuncs[triggerName] = lambda;
    return m_metTriggerFuncs.at(triggerName)();
  }
  // We can't get the exact trigger decision :( . Look for an alternative
  std::vector<std::string> replacementTriggers({"HLT_xe110_mht_L1XE50", "HLT_xe100_mht_L1XE50", "HLT_xe90_mht_L1XE50", "HLT_xe70_mht"});
  for (const std::string& trigName : replacementTriggers) {
    if (m_isTrigInTDT(trigName) ) {
      ATH_MSG_WARNING( "Trigger " << triggerName << " not available and direct emulation impossible! Will use " << trigName << " instead!");
      m_metTriggerFuncs[triggerName] = [this, trigName] () { 
        return m_trigDecTool->isPassed(trigName);
      };
      return m_metTriggerFuncs.at(triggerName)();
    }
  }
  ATH_MSG_ERROR( "Cannot find the trigger in the menu, direct emulation is impossible and no replacement triggers are available! Will return false" );
  m_metTriggerFuncs.at(triggerName) = [] () {return false;};
  return m_metTriggerFuncs.at(triggerName)();
}



bool SUSYObjDef_xAOD::IsTrigPassed(const std::string& tr_item, unsigned int condition) const {
  return m_trigDecTool->isPassed(tr_item, condition);
}


bool SUSYObjDef_xAOD::IsTrigMatched(const xAOD::IParticle *part, const std::string& tr_item) {
  return this->IsTrigMatched({part}, tr_item);
}


bool SUSYObjDef_xAOD::IsTrigMatched(const xAOD::IParticle *part1, const xAOD::IParticle *part2, const std::string& tr_item) {
  return this->IsTrigMatched({part1, part2}, tr_item);
}


bool SUSYObjDef_xAOD::IsTrigMatched(const std::vector<const xAOD::IParticle*>& v, const std::string& tr_item) {
  return m_trigMatchingTool->match(v, tr_item);
}


bool SUSYObjDef_xAOD::IsTrigMatched(const std::initializer_list<const xAOD::IParticle*> &v, const std::string& tr_item) {
  return m_trigMatchingTool->match(v, tr_item);
}


void SUSYObjDef_xAOD::TrigMatch(const xAOD::IParticle* p, std::initializer_list<std::string>::iterator i1, std::initializer_list<std::string>::iterator i2) {
  dec_trigmatched(*p) = 0;

  for(auto it = i1; it != i2; ++it) {
    auto result = static_cast<int>(this->IsTrigMatched(p, *it));
    dec_trigmatched(*p) += result;
    p->auxdecor<char>(*it) = result;
  }
}


void SUSYObjDef_xAOD::TrigMatch(const xAOD::IParticle* p, const std::initializer_list<std::string>& items) {
  this->TrigMatch(p, items.begin(), items.end());
}


void SUSYObjDef_xAOD::TrigMatch(const xAOD::IParticle* p, const std::vector<std::string>& items) {
  dec_trigmatched(*p) = 0;

  for(const auto& item: items) {
    auto result = static_cast<int>(this->IsTrigMatched(p, item));
    dec_trigmatched(*p) += result;
    p->auxdecor<char>(item) = result;
  }
}


void SUSYObjDef_xAOD::TrigMatch(const std::initializer_list<const xAOD::IParticle*> &v, const std::initializer_list<std::string> &items) {
  for(const auto& p : v) {
    this->TrigMatch(p, items);
  }
}


void SUSYObjDef_xAOD::TrigMatch(const std::initializer_list<const xAOD::IParticle*> &v, const std::vector<std::string>& items) {
  for(const auto& p : v) {
    this->TrigMatch(p, items);
  }
}


void SUSYObjDef_xAOD::TrigMatch(const xAOD::IParticleContainer* v, const std::vector<std::string>& items) {
  for(const auto& p : *v) {
    this->TrigMatch(p, items);
  }
}


void SUSYObjDef_xAOD::TrigMatch(const xAOD::IParticleContainer* v, const std::initializer_list<std::string>& items) {
  for(const auto& p : *v) {
    this->TrigMatch(p, items);
  }
}


void SUSYObjDef_xAOD::TrigMatch(const xAOD::IParticle* p, const std::string& item) {
  return this->TrigMatch(p, {item});
}


void SUSYObjDef_xAOD::TrigMatch(const xAOD::IParticleContainer* v,  const std::string& item) {
  return this->TrigMatch(v, {item});
}


void SUSYObjDef_xAOD::TrigMatch(const std::initializer_list<const xAOD::IParticle*> &v, const std::string& item) {
  return this->TrigMatch(v, {item});
}


float SUSYObjDef_xAOD::GetTrigPrescale(const std::string & tr_item) const {
  return m_trigDecTool->getPrescale(tr_item);
}


const Trig::ChainGroup* SUSYObjDef_xAOD::GetTrigChainGroup(const std::string& tr_item) const {
  return m_trigDecTool->getChainGroup(tr_item);
}


  std::vector<std::string> SUSYObjDef_xAOD::GetTriggerOR(std::string trigExpr) const {

    static std::string delOR = "_OR_";
    std::vector<std::string> trigchains = {};
    std::string newtrigExpr = TString(trigExpr).Copy().ReplaceAll("||",delOR).Data();
    newtrigExpr = TString(trigExpr).Copy().ReplaceAll(" ","").Data();
 
    size_t pos = 0;
    while ((pos = newtrigExpr.find(delOR)) != std::string::npos) {
      trigchains.push_back( "HLT_"+newtrigExpr.substr(0, pos) );
      newtrigExpr.erase(0, pos + delOR.length());
    }
    if(pos==std::string::npos)
      trigchains.push_back("HLT_"+newtrigExpr);
    
    return trigchains;
  }

  void SUSYObjDef_xAOD::GetTriggerTokens(std::string trigExpr, std::vector<std::string>& v_trigs15_cache, std::vector<std::string>& v_trigs16_cache, std::vector<std::string>& v_trigs17_cache) const {

    static std::string del15 = "_2015_";
    static std::string del16 = "_2016_";
    static std::string del17 = "_2017_";

    size_t pos = 0;
    std::string token15, token16, token17;

    //get trigger tokens for 2015, 2016 and 2017 
    if ( (pos = trigExpr.find(del15)) != std::string::npos) {
      trigExpr.erase(0, pos + del15.length()); 

      pos = 0;
      while ((pos = trigExpr.find(del16)) != std::string::npos) {
        token15 = trigExpr.substr(0, pos);
        token16 = trigExpr.erase(0, pos + del16.length() + del17.length());
        token17 = token16; // 2017 and 2016 use exact same trigger in string
      }
    }

    //redefine in case of custom user input
    if(token15.empty())
      token15 = trigExpr;

    if(token16.empty())
      token16 = trigExpr;

    if(token17.empty())
      token17 = trigExpr;

    //get trigger chains for matching in 2015 and 2016                                  
    v_trigs15_cache = GetTriggerOR(token15);
    v_trigs16_cache = GetTriggerOR(token16);
    v_trigs17_cache = GetTriggerOR(token17);
  }

  Trig::FeatureContainer SUSYObjDef_xAOD::GetTriggerFeatures(const std::string& chainName, unsigned int condition) const
  {
    return m_trigDecTool->features(chainName,condition);
  }

double SUSYObjDef_xAOD::GetTriggerGlobalEfficiencySF(const xAOD::ElectronContainer& electrons, const xAOD::MuonContainer& muons, const std::string& trigExpr) {

  double trig_sf(1.);

  if (trigExpr!="multiLepton" && trigExpr!="diLepton") {
    ATH_MSG_ERROR( "Failed to retrieve signal electron trigger SF");
    return trig_sf;
  }

  std::vector<const xAOD::Electron*> elec_trig;
  elec_trig.clear();
  for (const auto& electron : electrons) {
    if (!acc_passOR(*electron)) continue;
    if (!acc_signal(*electron)) continue;
    elec_trig.push_back(electron);
  }

  std::vector<const xAOD::Muon*> muon_trig;
  muon_trig.clear();
  for (const auto& muon : muons) {
    if (!acc_passOR(*muon)) continue;
    if (!acc_signal(*muon)) continue;
    muon_trig.push_back(muon);
  }

  bool matched = false;
  if ((elec_trig.size()+muon_trig.size())>1 && trigExpr=="diLepton") {
    if ( m_trigGlobalEffCorrTool_diLep->checkTriggerMatching( matched, elec_trig, muon_trig) != CP::CorrectionCode::Ok ) { 
      ATH_MSG_ERROR ("trigGlobEffCorrTool::Trigger matching could not be checked, interrupting execution.");
    }
  } else if ((elec_trig.size()+muon_trig.size())>2 && trigExpr=="multiLepton") {
    if ( m_trigGlobalEffCorrTool_multiLep->checkTriggerMatching( matched, elec_trig, muon_trig) != CP::CorrectionCode::Ok ) { 
      ATH_MSG_ERROR ("trigGlobEffCorrTool::Trigger matching could not be checked, interrupting execution.");
    }
  }

  CP::CorrectionCode result;
  if ((elec_trig.size()+muon_trig.size())>1 && trigExpr=="diLepton" && matched) {
    result = m_trigGlobalEffCorrTool_diLep->getEfficiencyScaleFactor( elec_trig, muon_trig, trig_sf);
  }
  else if ((elec_trig.size()+muon_trig.size())>2 && trigExpr=="multiLepton" && matched) {
    result = m_trigGlobalEffCorrTool_multiLep->getEfficiencyScaleFactor( elec_trig, muon_trig, trig_sf);
  }
 
  switch (result) {
  case CP::CorrectionCode::Error:
    ATH_MSG_ERROR( "Failed to retrieve signal lepton trigger efficiency");
    return 1.;
  case CP::CorrectionCode::OutOfValidityRange:
    ATH_MSG_VERBOSE( "OutOfValidityRange found for signal lepton trigger efficiency");
    return 1.;
  default:
    break;
  }

  return trig_sf;

}

double SUSYObjDef_xAOD::GetTriggerGlobalEfficiency(const xAOD::ElectronContainer& electrons, const xAOD::MuonContainer& muons, const std::string& trigExpr) {

  double trig_eff(1.);
  double trig_eff_data(1.);

  if (trigExpr!="multiLepton" && trigExpr!="diLepton") {
    ATH_MSG_ERROR( "Failed to retrieve signal electron trigger SF");
    return trig_eff;
  }

  std::vector<const xAOD::Electron*> elec_trig;
  elec_trig.clear();
  for (const auto& electron : electrons) {
    if (!acc_passOR(*electron)) continue;
    if (!acc_signal(*electron)) continue;
    elec_trig.push_back(electron);
  }

  std::vector<const xAOD::Muon*> muon_trig;
  muon_trig.clear();
  for (const auto& muon : muons) {
    if (!acc_passOR(*muon)) continue;
    if (!acc_signal(*muon)) continue;
    muon_trig.push_back(muon);
  }

  bool matched = false;
  if ((elec_trig.size()+muon_trig.size())>1 && trigExpr=="diLepton") {
    if ( m_trigGlobalEffCorrTool_diLep->checkTriggerMatching( matched, elec_trig, muon_trig) != CP::CorrectionCode::Ok ) { 
      ATH_MSG_ERROR ("trigGlobEffCorrTool::Trigger matching could not be checked, interrupting execution.");
    }
  } else if ((elec_trig.size()+muon_trig.size())>2 && trigExpr=="multiLepton") {
    if ( m_trigGlobalEffCorrTool_multiLep->checkTriggerMatching( matched, elec_trig, muon_trig) != CP::CorrectionCode::Ok ) { 
      ATH_MSG_ERROR ("trigGlobEffCorrTool::Trigger matching could not be checked, interrupting execution.");
    }
  }

  CP::CorrectionCode result;
  if ((elec_trig.size()+muon_trig.size())>1 && trigExpr=="diLepton" && matched) {
    result = m_trigGlobalEffCorrTool_diLep->getEfficiency( elec_trig, muon_trig, trig_eff_data, trig_eff);
  }
  else if ((elec_trig.size()+muon_trig.size())>2 && trigExpr=="multiLepton" && matched) {
    result = m_trigGlobalEffCorrTool_multiLep->getEfficiency( elec_trig, muon_trig, trig_eff_data, trig_eff);
  }
 
  switch (result) {
  case CP::CorrectionCode::Error:
    ATH_MSG_ERROR( "Failed to retrieve signal lepton trigger efficiency");
    return 1.;
  case CP::CorrectionCode::OutOfValidityRange:
    ATH_MSG_VERBOSE( "OutOfValidityRange found for signal lepton trigger efficiency");
    return 1.;
  default:
    break;
  }

  if (isData()) return trig_eff_data;
  else return trig_eff;

}

}

