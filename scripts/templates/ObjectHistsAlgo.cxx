#include <EventLoop/Job.h>
#include <EventLoop/StatusCode.h>
#include <EventLoop/Worker.h>

#include "xAODJet/JetContainer.h"
#include "xAODTracking/VertexContainer.h"
#include "xAODEventInfo/EventInfo.h"

#include <${package}/${name}Hists.h>
#include <${package}/${name}HistsAlgo.h>
#include <xAODAnaHelpers/HelperFunctions.h>

#include "TEnv.h"
#include "TSystem.h"

// this is needed to distribute the algorithm to the workers
ClassImp(${name}HistsAlgo)

${name}HistsAlgo :: ${name}HistsAlgo () {
}

${name}HistsAlgo :: ${name}HistsAlgo (std::string name, std::string configName) :
  Algorithm(),
  m_name(name),
  m_configName(configName),
  m_plots(0)
{
}

EL::StatusCode ${name}HistsAlgo :: setupJob (EL::Job& job)
{
  job.useXAOD();
  xAOD::Init("${name}HistsAlgo").ignore();

  return EL::StatusCode::SUCCESS;
}

EL::StatusCode ${name}HistsAlgo :: histInitialize ()
{

  Info("histInitialize()", "%s", m_name.c_str() );
  // needed here and not in initalize since this is called first
  Info("histInitialize()", "Attempting to configure using: %s", m_configName.c_str());
  if ( this->configure() == EL::StatusCode::FAILURE ) {
    Error("histInitialize()", "%s failed to properly configure. Exiting.", m_name.c_str() );
    return EL::StatusCode::FAILURE;
  } else {
    Info("histInitialize()", "Succesfully configured! \n");
  }

  // declare class and add histograms to output
  m_plots = new ${name}Hists(m_name, m_detailStr);
  m_plots -> initialize( );
  m_plots -> record( wk() );

  return EL::StatusCode::SUCCESS;
}

EL::StatusCode ${name}HistsAlgo :: configure ()
{
  m_configName = gSystem->ExpandPathName( m_configName.c_str() );
  // check if file exists
  /* https://root.cern.ch/root/roottalk/roottalk02/5332.html */
  FileStat_t fStats;
  int fSuccess = gSystem->GetPathInfo(m_configName.c_str(), fStats);
  if(fSuccess != 0){
    Error("configure()", "Could not find the configuration file");
    return EL::StatusCode::FAILURE;
  }
  Info("configure()", "Found configuration file");

  // the file exists, use TEnv to read it off
  TEnv* config = new TEnv(m_configName.c_str());
  m_inContainerName         = config->GetValue("InputContainer",  "");
  m_detailStr               = config->GetValue("DetailStr",       "");

  // in case anything was missing or blank...
  if( m_inContainerName.empty() || m_detailStr.empty() ){
    Error("configure()", "One or more required configuration values are empty");
    return EL::StatusCode::FAILURE;
  }
  Info("configure()", "Loaded in configuration values");

  // everything seems preliminarily ok, let's print config and say we were successful
  config->Print();
  return EL::StatusCode::SUCCESS;
}

EL::StatusCode ${name}HistsAlgo :: fileExecute () { return EL::StatusCode::SUCCESS; }
EL::StatusCode ${name}HistsAlgo :: changeInput (bool /*firstFile*/) { return EL::StatusCode::SUCCESS; }

EL::StatusCode ${name}HistsAlgo :: initialize ()
{
  Info("initialize()", "${name}HistsAlgo");
  m_event = wk()->xaodEvent();
  m_store = wk()->xaodStore();
  return EL::StatusCode::SUCCESS;
}

EL::StatusCode ${name}HistsAlgo :: execute ()
{
  const xAOD::EventInfo* eventInfo = 0;
  if ( ! m_event->retrieve(eventInfo, "EventInfo").isSuccess() ) {
    Error("AnalysisLoop::execute()", "Failed to retrieve event info collection. Exiting.");
    return EL::StatusCode::FAILURE;
  }

  float eventWeight(1);
  if( eventInfo->isAvailable< float >( "eventWeight" ) ) {
    eventWeight = eventInfo->auxdecor< float >( "eventWeight" );
  }

  const xAOD::JetContainer* jets = 0;
  if ( !m_event->retrieve( jets, m_inContainerName ).isSuccess() ){
    if ( !m_store->retrieve( jets, m_inContainerName ).isSuccess() ){
      Error("${name}HistsAlgo::execute()  ", "Failed to retrieve %s container. Exiting.", m_inContainerName.c_str() );
      return EL::StatusCode::FAILURE;
    }
  }

  /* two ways to fill */

  // 1. pass the jet collection
  m_plots->execute( jets, eventWeight );

  /* 2. loop over the jets
  xAOD::${name}Container::const_iterator jet_itr = jets->begin();
  xAOD::${name}Container::const_iterator jet_end = jets->end();
  for( ; jet_itr != jet_end; ++jet_itr ) {
    m_plots->execute( *jet_itr, eventWeight );
  }
  */

  return EL::StatusCode::SUCCESS;
}

EL::StatusCode ${name}HistsAlgo :: postExecute () { return EL::StatusCode::SUCCESS; }
EL::StatusCode ${name}HistsAlgo :: finalize () { return EL::StatusCode::SUCCESS; }

EL::StatusCode ${name}HistsAlgo :: histFinalize () {
  // clean up memory
  if(m_plots){
    delete m_plots;
    m_plots = 0;
  }
  return EL::StatusCode::SUCCESS;
}