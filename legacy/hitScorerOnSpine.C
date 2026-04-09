#pragma once 

#include "gallery/Event.h"
#include "canvas/Persistency/Common/Wrapper.h"
#include "lardataobj/RecoBase/Hit.h"
#include "lardataobj/RecoBase/Wire.h"
#include "canvas/Utilities/InputTag.h"
#include "gallery/ValidHandle.h"
#include "gallery/Handle.h"
#include "canvas/Persistency/Common/FindMany.h"
#include "canvas/Persistency/Common/FindOne.h"
#include "canvas/Persistency/Common/FindManyP.h"
#include "canvas/Persistency/Common/fwd.h"
#include "canvas/Persistency/Common/Ptr.h"
#include "canvas/Persistency/Provenance/Timestamp.h"
#include "art/Framework/Principal/Event.h"
#include "nusimdata/SimulationBase/MCTruth.h"
#include "lardataobj/AnalysisBase/BackTrackerMatchingData.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "lardata/DetectorInfoServices/DetectorClocksService.h"
#include "nusimdata/SimulationBase/MCParticle.h"
#include "larsim/MCCheater/ParticleInventoryService.h"
#include "larsim/Utils/TruthMatchUtils.h"
#include "lardataobj/Simulation/SimChannel.h"
#include "sbnobj/ICARUS/TPC/ChannelROI.h"
#include "TCanvas.h"
#include <iostream>
#include <vector>
#include "TH1F.h"
#include "TH2F.h"
#include "TTree.h"
#include "TFile.h"
#include "TStyle.h"
#include "TROOT.h"
#include "TLegend.h"
#include <numeric>


class planeRecord: public TObject
{
    public:
    planeRecord() 
      : hitEnergy(0.0),
        ideEnergy(0.0),
        planeRatio(0.0),
        numberOfHits(0)
    {};

    double hitEnergy, ideEnergy, planeRatio; 
    int numberOfHits;

    std::vector<double> 
        hitPeakAmplitude,
        hitRms,
        hitIntegral,
        hitGoodnessOfFit,
        hitFit,
        hitHitSummedAdc,
        hitRoiSummedAdc,
        hitChannel,
        hitAreaRatio;

    std::vector<int> 
        pdgCode;

    std::vector<double>
        hitEnergies,
        hitIdeFraction;

    ClassDef(planeRecord, 1);
};

class eventRecord: public TObject
{
    public:
    eventRecord() = default;
    eventRecord(unsigned event, unsigned subrun, unsigned run)
      : event(event), subrun(subrun), run(run),
        primaryProtons(0), primaryChargedPions(0), primaryMuons(0),
        primaryElectrons(0), primaryNeutralPions(0), primaryGammas(0),
        protons(0), chargedPions(0), muons(0),
        electrons(0), neutralPions(0), gammas(0),
        eventRatio(0.0)
    {};
    unsigned event, subrun, run;

    unsigned 
        primaryProtons, primaryChargedPions, primaryMuons,
        primaryElectrons, primaryNeutralPions, primaryGammas;
    
    unsigned 
        protons, chargedPions, muons,
        electrons, neutralPions, gammas;

    double maxEnergyTheta, maxEnergyPhi; 
    
    double eventRatio;

    planeRecord p0, p1, p2;

    ClassDef(eventRecord, 1);
};

int getPlane(int channelID)
{
    if (channelID >= 0 && channelID <= 2239) return 0;
    else if (channelID >= 13824 && channelID <= 16063) return 0;
    else if (channelID >= 27648 && channelID <= 29887) return 0;
    else if (channelID >= 41472 && channelID <= 43711) return 0;

    else if (channelID >= 2240 && channelID <= 8063) return 1;
    else if (channelID >= 16128 && channelID <= 21087) return 1;
    else if (channelID >= 29952 && channelID <= 35711) return 1;
    else if (channelID >= 43776 && channelID <= 49535) return 1;

    else if (channelID >= 8064 && channelID <= 13823) return 2;
    else if (channelID >= 21888 && channelID <= 27647) return 2;
    else if (channelID >= 35712 && channelID <= 41471) return 2;
    else if (channelID >= 49536 && channelID <= 55295) return 2;

    return -1; // invalid channel
}

const auto safeDivide = [](float a, float b) -> float {
    return b != 0.0f ? a / b : 0.0f;
};

std::vector<std::string> files = {
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/bc/47/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953889_0-55b017d1-99e0-439d-a906-4154b5aa9ac6.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/21/df/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953888_0-4f4c3b86-fd8d-465b-9ee9-2082db08bb82.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/26/37/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953890_0-be69702d-d115-49da-9454-ecaed3404173.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/b6/55/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953892_0-34a3e931-4b74-4359-99d0-2729378a355e.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/29/1f/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953891_0-5af2ee57-7f2e-44e8-ab6b-9427240e6d4b.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/8a/23/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953885_0-b5f4d1be-536f-45dd-883e-1a8996b53a85.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/b2/db/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953884_0-05caee47-2d15-412a-97dd-4debd0ce81a9.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/ef/fa/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953886_0-8d0e61e7-907d-45ab-b400-ef48220769d5.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/b7/f9/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953887_0-57a8fa67-6212-47b1-b74a-9167dd320f8c.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/f0/2c/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953893_0-ff01029d-c0c2-46d7-8b6b-b35d4f5917a1.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/7f/0f/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953898_0-8740287f-d5fa-410b-8c10-d6aea8082d13.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/ee/e2/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953911_0-4feae724-e7a4-4819-8141-94648460a5ef.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/7a/e9/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953894_0-5d509b53-b07a-45dc-8052-335614b8aa35.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/36/25/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953909_0-3a3a96cf-1282-4846-816d-d41518e76745.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/03/23/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953915_0-55979dab-fa10-492b-a20c-d9d6be4da6be.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/98/dd/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953904_0-ca9165a9-d59c-4ad9-bd0f-6b0e7bac4bf0.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/93/81/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953905_0-7b141def-d90a-444c-af07-aa4e15a4f216.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/0d/c6/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953910_0-8bcc62d1-5cd5-4b6a-8826-a4723962fe41.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/de/0d/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953899_0-a55b5361-21f4-4a7e-870f-9cf41b7ef6a1.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/1d/d4/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953906_0-a47d8b0a-18b6-4fca-808c-138d5e3d643f.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/61/22/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953917_0-d9017274-0c6f-44c1-8b3a-c65615e00ca8.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/9f/ef/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953907_0-efffa1dd-76e5-4535-9574-3d1079b7c920.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/b2/30/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953900_0-8ee7ffc5-0cf7-421c-b4c9-df6bbbec99c8.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/00/3b/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953918_0-383c2a26-e80a-430f-b41c-e145ced19a6e.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/26/f9/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953912_0-3b89e69f-843e-47f0-a32b-ed8876c39463.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/c1/25/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953929_0-4ae632c6-43f6-41d7-a3e9-b7237e755dd4.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/77/a1/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953908_0-aef24225-7337-43f1-8916-3ac9c8f67b7a.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/ab/f8/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953901_0-66968e99-7896-411f-9a68-15d6a3fcaea9.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/d1/10/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953914_0-1c8ab9d2-fc5b-4dde-813e-3fd6ea2c4698.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/26/01/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953913_0-4158d9ca-5843-49fe-a493-79fcde7fbe0a.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/77/36/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953916_0-3dadef9a-8e7c-4a2a-9b66-0de70906344f.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/29/5b/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953895_0-d3dc2a40-3260-4d22-8f40-8525d78d451b.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/1d/97/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953903_0-8f29b7f0-330b-45b3-a5b7-68951a110466.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/34/b8/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953902_0-08971e87-db45-43d5-94bc-434d26c077b6.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/a7/1e/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953896_0-c6864c2e-7d55-4972-8932-15c65f2e9eb2.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/e6/ec/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953922_0-ad3e5584-dc68-4fb0-b391-196e7cdfc223.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/fc/c1/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953920_0-51280a23-2f09-480c-8606-2aa691fd2877.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/28/9c/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953926_0-81b7684c-9b27-495c-a374-5ca128b7249a.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/9d/94/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953928_0-0b87ad93-4663-4ba6-977f-4fb6d55106d3.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/49/db/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953924_0-999956d3-46a7-4e37-b823-847b64555577.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/d4/23/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953931_0-1588231a-4768-4659-bcd0-41a089ba4c45.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/72/03/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953921_0-dfa111c1-b499-4bb2-ac3a-63f4e838c1dc.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/f8/d2/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953927_0-bc8c92ac-6113-4144-baba-18c3f9ee7e80.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/9a/31/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953925_0-02f39c43-c92f-447d-941a-8386c7c47951.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/d4/c2/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953932_0-aece6f54-2a3b-437e-a10b-c0961dff727c.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/d9/6e/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953897_0-c1ef3ab3-760c-4511-8590-7104f88e4395.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/ce/61/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953930_0-08ba3ccc-c1f4-4612-a800-3ba7d792927e.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/c3/5b/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953933_0-0f5808ea-036e-4b6a-b0b4-f8f8ee3cc715.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/87/11/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953923_0-3d352e50-47c9-446d-8ed9-5251f19adc53.root", 
    "/pnfs/icarus/scratch/users/msotgia/hitTuning/v10_06_00_06p03/overlayRun9435/Respun2/SPINESettings/Stage1ToCAF/out/1f/bb/msotgia_v10_06_00_06p03_overlay_run9435_A_bnb_withOverlayFixed_spine_v10_06_00_06p03_stage1_89953919_0-d3ec9ef0-c2f1-401e-a8e0-2a3a5bf7732c.root"
};

void hitScorerOnSpine(std::vector<std::string> f = files, std::string writerName = "tmp/testOneGalleryMetric/scorerTest.root")
{

    std::vector<std::string> allFiles = f;

    // Create gallery event loop
    gallery::Event event(allFiles);
    std::cout << "Starting hitScorer with # " 
        << event.numberOfEventsInFile() << " event(s)" << std::endl;

    std::unique_ptr<TFile> writer(new TFile(writerName.c_str(), "RECREATE"));
    std::cout << "Created writer file " << writerName << std::endl;
    std::unique_ptr<TTree> data(new TTree("data", ""));
    std::cout << "Created tree data" << std::endl;

    eventRecord thisEvent;
    data->Branch("thisEvent", &thisEvent);
    std::cout << "Set Tree to write events" << std::endl;

    std::vector<eventRecord> allEvents;

    for (; !event.atEnd(); event.next())
    {

        eventRecord eventTmp
            (event.eventAuxiliary().event(), event.eventAuxiliary().subRun(), event.eventAuxiliary().run()); 

        // std::cout << "Created temporary eventRecord" << std::endl;

        std::array<planeRecord, 3> mappedPlanes;

        auto const &hitMCParticleAssns = 
            *event.getValidHandle<art::Assns<recob::Hit, simb::MCParticle, anab::BackTrackerHitMatchingData>>("mcassociationsGausCryoE");

        auto const& mcHandle = 
            *event.getValidHandle<std::vector<simb::MCParticle>>("largeant");

        
        int neutrinoId;
        for (auto const& mcParticle: mcHandle) 
        {
            if (mcParticle.Mother() == -1)
            {
                neutrinoId = mcParticle.TrackId();
                break;
            }
        }

        for (auto const& mcParticle: mcHandle) 
        {

            if (std::abs(mcParticle.PdgCode()) == 2212)
            {
                if (mcParticle.Mother() == neutrinoId)
                    eventTmp.primaryProtons++;
                else
                    eventTmp.protons++;
            }

            if (std::abs(mcParticle.PdgCode()) == 211)
            {
                if (mcParticle.Mother() == neutrinoId)
                    eventTmp.primaryChargedPions++;
                else
                    eventTmp.chargedPions++;
            }

            if (std::abs(mcParticle.PdgCode()) == 13)
            {
                if (mcParticle.Mother() == neutrinoId)
                    eventTmp.primaryMuons++;
                else
                    eventTmp.muons++;
            }

            if (std::abs(mcParticle.PdgCode()) == 11)
            {
                if (mcParticle.Mother() == neutrinoId)
                    eventTmp.primaryElectrons++;
                else
                    eventTmp.electrons++;
            }

            if (std::abs(mcParticle.PdgCode()) == 111)
            {
                if (mcParticle.Mother() == neutrinoId)
                    eventTmp.primaryNeutralPions++;
                else
                    eventTmp.neutralPions++;
            }

            if (std::abs(mcParticle.PdgCode()) == 22)
            {
                if (mcParticle.Mother() == neutrinoId)
                    eventTmp.primaryGammas++;
                else
                    eventTmp.gammas++;
            }
        }
        
        auto const& simHandle = 
            *event.getValidHandle<std::vector<sim::SimChannel>>("merge");

        // auto const& hitsWW =
        //    *event.getValidHandle<std::vector<recob::Hit>>({"gaushit2dTPCWW"});
        // auto const& hitsWE =
        //    *event.getValidHandle<std::vector<recob::Hit>>({"gaushit2dTPCWE"});
        // auto const& hitsEW =
        //    *event.getValidHandle<std::vector<recob::Hit>>({"gaushit2dTPCEW"});
        // auto const& hitsEE =
        //    *event.getValidHandle<std::vector<recob::Hit>>({"gaushit2dTPCEE"});

        // std::vector< const std::vector<recob::Hit>* > allHits = 
        //     {&hitsWW, &hitsWE, &hitsEW, &hitsEE};
        
        for (auto it = hitMCParticleAssns.begin(); it != hitMCParticleAssns.end(); ++it)
        {
            auto const& [hit, mcParticle, _] = *it;
            auto const& matchData = hitMCParticleAssns.data(it);
            int plane = hit->WireID().getIndex<2>();
            if (plane < 0 || plane > 2) continue;

            mappedPlanes[plane].hitPeakAmplitude.push_back(hit->PeakAmplitude());
            mappedPlanes[plane].hitRms.push_back(hit->RMS());
            mappedPlanes[plane].hitIntegral.push_back(hit->Integral());
            mappedPlanes[plane].hitGoodnessOfFit.push_back(hit->GoodnessOfFit());
            mappedPlanes[plane].hitFit.push_back(hit->GoodnessOfFit()/hit->DegreesOfFreedom());
            mappedPlanes[plane].hitHitSummedAdc.push_back(hit->HitSummedADC());
            mappedPlanes[plane].hitRoiSummedAdc.push_back(hit->ROISummedADC());
            mappedPlanes[plane].hitChannel.push_back(hit->WireID().getIndex<3>());
            mappedPlanes[plane].hitAreaRatio.push_back(hit->Integral()/hit->HitSummedADC());

            mappedPlanes[plane].pdgCode.push_back(mcParticle->PdgCode());
            mappedPlanes[plane].hitEnergies.push_back(matchData.energy);
            mappedPlanes[plane].hitIdeFraction.push_back(matchData.ideFraction);
            mappedPlanes[plane].numberOfHits++;

            mappedPlanes[plane].hitEnergy += matchData.energy * matchData.ideFraction;
        }

        for (auto const& sc: simHandle)
        {
            int plane = getPlane(sc.Channel());
            if (plane < 0 || plane > 2) continue;
            for (auto const& [tdc, ides]: sc.TDCIDEMap())
            {
                for (auto const& ide: ides)
                {
                    mappedPlanes[plane].ideEnergy += ide.energy;
                }
            }
        }

        for (unsigned plane=0; plane<3; plane++)
        {
            mappedPlanes[plane].planeRatio = 
                safeDivide(mappedPlanes[plane].hitEnergy, mappedPlanes[plane].ideEnergy);
        }

        eventTmp.p0 = std::move(mappedPlanes[0]);
        eventTmp.p1 = std::move(mappedPlanes[1]);
        eventTmp.p2 = std::move(mappedPlanes[2]);

        eventTmp.eventRatio = 
            safeDivide(
                eventTmp.p0.hitEnergy + eventTmp.p1.hitEnergy + eventTmp.p2.hitEnergy,
                eventTmp.p0.ideEnergy + eventTmp.p1.ideEnergy + eventTmp.p2.ideEnergy
            );
        
        allEvents.push_back(eventTmp);

        thisEvent = std::move(eventTmp);
        data->Fill();
    }
    
    writer->cd();
    data->Write();

    std::cout << "File is closed" << std::endl;
}
