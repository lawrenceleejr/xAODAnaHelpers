/******************************************
 *
 * This class builds HLT jets and thier associated objects
 *
 * John Alison (john.alison@cern.ch)
 *
 *
 ******************************************/

// c++ include(s):
#include <iostream>
#include <vector>

// EL include(s):
#include <EventLoop/Job.h>
#include <EventLoop/StatusCode.h>
#include <EventLoop/Worker.h>

// EDM include(s):
#include "xAODEventInfo/EventInfo.h"
#include "xAODJet/JetContainer.h"
#include "xAODJet/JetAuxContainer.h"
#include "xAODJet/Jet.h"
#include "xAODBase/IParticleHelpers.h"
#include "xAODBase/IParticleContainer.h"
#include "xAODBase/IParticle.h"
#include "AthContainers/ConstDataVector.h"
#include "AthContainers/DataVector.h"
#include "xAODCore/ShallowCopy.h"

// package include(s):
#include "xAODAnaHelpers/HelperFunctions.h"
#include "xAODAnaHelpers/HLTJetRoIBuilder.h"
#include <xAODAnaHelpers/tools/ReturnCheck.h>

#include "TrigConfxAOD/xAODConfigTool.h"
#include "TrigDecisionTool/TrigDecisionTool.h"

using std::cout;  using std::endl;
using std::vector;

// this is needed to distribute the algorithm to the workers
ClassImp(HLTJetRoIBuilder)

HLTJetRoIBuilder :: HLTJetRoIBuilder (std::string className) :
  Algorithm(className),
  m_trigItem(""),
  m_doHLTBJet(true),
  m_doHLTJet (false),
  m_outContainerName(""),
  m_trigDecTool(nullptr)
{
  if(m_debug) Info("HLTJetRoIBuilder()", "Calling constructor");

  // read debug flag from .config file
  m_debug                   = false;

  m_sort                    = true;

}


EL::StatusCode HLTJetRoIBuilder :: setupJob (EL::Job& job)
{
  if(m_debug) Info("setupJob()", "Calling setupJob");
  job.useXAOD ();
  xAOD::Init( "HLTJetRoIBuilder" ).ignore(); // call before opening first file
  return EL::StatusCode::SUCCESS;
}



EL::StatusCode HLTJetRoIBuilder :: histInitialize ()
{
  RETURN_CHECK("xAH::Algorithm::algInitialize()", xAH::Algorithm::algInitialize(), "");
  return EL::StatusCode::SUCCESS;
}



EL::StatusCode HLTJetRoIBuilder :: fileExecute ()
{
  return EL::StatusCode::SUCCESS;
}



EL::StatusCode HLTJetRoIBuilder :: changeInput (bool /*firstFile*/)
{
  return EL::StatusCode::SUCCESS;
}



EL::StatusCode HLTJetRoIBuilder :: initialize ()
{

  if(m_debug) Info("initialize()", "Initializing HLTJetRoIBuilder Interface... ");

  m_event = wk()->xaodEvent();
  m_store = wk()->xaodStore();

  //
  // Grab the TrigDecTool from the ToolStore
  //
  m_trigDecTool = dynamic_cast<Trig::TrigDecisionTool*>(asg::ToolStore::get("TrigDecisionTool"));

  return EL::StatusCode::SUCCESS;
}


EL::StatusCode HLTJetRoIBuilder :: execute ()
{
  if ( m_debug ) { Info("execute()", "Doing HLT JEt ROI Building... "); }

  if(m_doHLTBJet){
    return buildHLTBJets();
  }else if(m_doHLTJet){
    return buildHLTJets();
  }



  if ( m_debug ) { m_store->print(); }

  return EL::StatusCode::SUCCESS;
}



EL::StatusCode HLTJetRoIBuilder :: buildHLTBJets ()
{
  //
  // Create the new container and its auxiliary store.
  //
  xAOD::JetContainer*     hltJets    = new xAOD::JetContainer();
  xAOD::JetAuxContainer*  hltJetsAux = new xAOD::JetAuxContainer();
  hltJets->setStore( hltJetsAux ); //< Connect the two

  //
  //  For Adding Tracks to the Jet
  //
  xAOD::Jet::Decorator<vector<const xAOD::TrackParticle*> > m_track_decoration("HLTBJetTracks");
  xAOD::Jet::Decorator<const xAOD::Vertex*>                 m_vtx_decoration  ("HLTBJetTracks_vtx");

  //
  //  Make accessors/decorators
  //
  static SG::AuxElement::ConstAccessor< vector<ElementLink<DataVector<xAOD::IParticle> > > > jetLinkAcc("BTagBtagToJetAssociator");
  static SG::AuxElement::ConstAccessor< vector<ElementLink<xAOD::TrackParticleContainer> > > trkLinkAcc("BTagTrackToJetAssociator");
  static SG::AuxElement::Decorator< const xAOD::BTagging* > hltBTagDecor( "HLTBTag" );

  Trig::FeatureContainer fc = m_trigDecTool->features(m_trigItem);
  vector<Trig::Feature<xAOD::BTaggingContainer> > bjetFeatureContainers = fc.containerFeature<xAOD::BTaggingContainer>();


  //
  //  Gettign the Primary Vertex
  //
  vector<Trig::Feature<xAOD::VertexContainer> > vtxFeatureContainers     = fc.containerFeature<xAOD::VertexContainer>("xPrimVx");
  vector<Trig::Feature<xAOD::VertexContainer> > histVtxFeatureContainers = fc.containerFeature<xAOD::VertexContainer>("EFHistoPrmVtx");

  const xAOD::Vertex*               pvx = 0;
  if(vtxFeatureContainers.size() == 1){
    pvx = HelperFunctions::getPrimaryVertex(vtxFeatureContainers.at(0).cptr());
  }else if(histVtxFeatureContainers.size() == 1){
    pvx = HelperFunctions::getPrimaryVertex(histVtxFeatureContainers.at(0).cptr());
  }else{
    cout << "ERROR Vertex size not 1: " << vtxFeatureContainers.size() << " " << histVtxFeatureContainers.size() << " " << m_name << endl;
    if(vtxFeatureContainers.size() > 0){
      pvx = HelperFunctions::getPrimaryVertex(vtxFeatureContainers.at(0).cptr());
    }else if(histVtxFeatureContainers.size() > 0){
      pvx = HelperFunctions::getPrimaryVertex(histVtxFeatureContainers.at(0).cptr());
    }
  }

  //
  //  Loop on ROIs
  //
  if(m_debug) cout << "ncontainers  " << bjetFeatureContainers.size() << endl;
  unsigned int nTrigROIs = bjetFeatureContainers.size();
  for(unsigned int iROI = 0; iROI < nTrigROIs; ++iROI){

    const xAOD::BTaggingContainer* btagCont = bjetFeatureContainers.at(iROI).cptr();
    if(!btagCont->size() == 1){
      cout << "ERROR BTaggingContainer size " << btagCont->size();
      continue;
    }

    const xAOD::BTagging*  hlt_btag = btagCont->at(0);

    bool isAvailableJet = jetLinkAcc.isAvailable(*hlt_btag);
    if(!isAvailableJet){
      cout << "ERROR Jet Link Acc not availible " << endl;;
      continue;
    }

    vector<ElementLink<DataVector<xAOD::IParticle> > > jetLinkObj = jetLinkAcc(*hlt_btag);
    if(m_debug) cout << "Filling " << jetLinkObj.size() << " jets ... " <<endl;

    if(!jetLinkObj.size()) {
      cout << "ERROR Jet Link Acc empty " << endl;;
      continue;
    }

    if(!jetLinkObj.at(0).isValid()) {
      cout << "ERROR Jet Link inValid " << endl;;
      continue;
    }

    if(m_debug) cout << "Casting "  << endl;
    const xAOD::IParticle* iPart = *(jetLinkObj.at(0));

    const xAOD::Jet* hltBJet = dynamic_cast<const xAOD::Jet*>(iPart);
    if(m_debug) cout << "Adding hltBJet " << hltBJet << " " << hlt_btag << endl;

    xAOD::Jet* newHLTBJet = new xAOD::Jet();
    newHLTBJet->makePrivateStore( hltBJet );
    
    //
    // Add Link to BTagging Info
    //
    newHLTBJet->auxdecor< const xAOD::BTagging* >("HLTBTag") = hlt_btag;
    if(m_debug) cout << "Added link " << endl;

    //
    // Add Tracks to BJet
    //
    bool isAvailableTrks = trkLinkAcc.isAvailable(*hlt_btag);
    if(isAvailableTrks){
      vector<ElementLink<xAOD::TrackParticleContainer> > trkLinkObj = trkLinkAcc(*hlt_btag);
      if(m_debug) cout << "trkLinkObj has " << trkLinkObj.size() << " objects" << endl;

      vector<const xAOD::TrackParticle*> matchedTracks;
      
      if(m_debug)cout << "Trk Size" << trkLinkObj.size() << endl;
      for(auto& trkPtr: trkLinkObj){
        const xAOD::TrackParticle* thisHLTTrk = *(trkPtr);
        if(m_debug) cout <<  "\tAdding  track "
                         << thisHLTTrk->pt()   << " "
                         << thisHLTTrk->eta()  << " "
                         << thisHLTTrk->phi()  << endl;
        matchedTracks.push_back(thisHLTTrk);
      }

      if(m_debug) cout <<  "Adding tracks to jet " << endl;
      m_track_decoration(*newHLTBJet)  = matchedTracks;

    }else{
      if(m_debug) cout << " Trks Not Avalible." << endl;
    }

    //
    //  Add the Vertex
    //
    m_vtx_decoration  (*newHLTBJet)  = pvx;

    hltJets->push_back( newHLTBJet );
    if(m_debug) cout << "pushed back " << endl;

  }//ROIs

  RETURN_CHECK("PlotHLTBJetFex::selected()", m_store->record( hltJets,    m_outContainerName),     "Failed to record selected dijets");
  RETURN_CHECK("PlotHLTBJetFex::selected()", m_store->record( hltJetsAux, m_outContainerName+"Aux."), "Failed to record selected dijetsAux.");
  return EL::StatusCode::SUCCESS;
}



EL::StatusCode HLTJetRoIBuilder :: buildHLTJets ()
{
  if(m_debug) cout << "In buildHLTJets  " <<endl;
  //
  // Create the new container and its auxiliary store.
  //
  xAOD::JetContainer*     hltJets    = new xAOD::JetContainer();
  xAOD::JetAuxContainer*  hltJetsAux = new xAOD::JetAuxContainer();
  hltJets->setStore( hltJetsAux ); //< Connect the two

  Trig::FeatureContainer fc = m_trigDecTool->features(m_trigItem);
  auto jetFeatureContainers = fc.containerFeature<xAOD::JetContainer>();

  if(m_debug) cout << "ncontainers  " << jetFeatureContainers.size() << endl;

  //DataModel_detail::const_iterator<JetContainer >::reference {aka const xAOD::Jet_v1*}

  for(auto  jcont : jetFeatureContainers) {
    for (const xAOD::Jet*  hlt_jet : *jcont.cptr()) {

      xAOD::Jet* newHLTJet = new xAOD::Jet();
      newHLTJet->makePrivateStore( hlt_jet );

      hltJets->push_back( newHLTJet );
    }
  }

  RETURN_CHECK("PlotHLTBJetFex::selected()", m_store->record( hltJets,    m_outContainerName),     "Failed to record selected dijets");
  RETURN_CHECK("PlotHLTBJetFex::selected()", m_store->record( hltJetsAux, m_outContainerName+"Aux."), "Failed to record selected dijetsAux.");
  if(m_debug) cout << "Left buildHLTJets  " <<endl;
  return EL::StatusCode::SUCCESS;
}



EL::StatusCode HLTJetRoIBuilder :: postExecute ()
{
  if ( m_debug ) { Info("postExecute()", "Calling postExecute"); }
  return EL::StatusCode::SUCCESS;
}



EL::StatusCode HLTJetRoIBuilder :: finalize ()
{
  if(m_debug) Info("finalize()", "Deleting tool instances...");
  return EL::StatusCode::SUCCESS;
}



EL::StatusCode HLTJetRoIBuilder :: histFinalize ()
{
  if(m_debug) Info("histFinalize()", "Calling histFinalize");
  RETURN_CHECK("xAH::Algorithm::algFinalize()", xAH::Algorithm::algFinalize(), "");
  return EL::StatusCode::SUCCESS;
}
