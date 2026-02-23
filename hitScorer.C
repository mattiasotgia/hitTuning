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

    std::vector<double> 
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
    double event, subrun, run;

    double 
        primaryProtons, primaryChargedPions, primaryMuons,
        primaryElectrons, primaryNeutralPions, primaryGammas;
    
    double 
        protons, chargedPions, muons,
        electrons, neutralPions, gammas;

    double maxEnergyTheta, maxEnergyPhi; 
    
    double hitEnergy, ideEnergy, eventRatio;

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
    return b != 0.0f ? a / b : -1.0f;
};

std::vector<std::vector<double>> hitScorer(std::string file, std::string writerName = "tmp/testOneGalleryMetric/scorerTest.root")
{

    std::vector<std::string> allFiles = {file};

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

        std::cout << "Created temporary eventRecord" << std::endl;

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

        eventTmp.hitEnergy = eventTmp.p0.hitEnergy + eventTmp.p1.hitEnergy + eventTmp.p2.hitEnergy;
        eventTmp.ideEnergy = eventTmp.p0.ideEnergy + eventTmp.p1.ideEnergy + eventTmp.p2.ideEnergy;

        eventTmp.eventRatio = 
            safeDivide(eventTmp.hitEnergy, eventTmp.ideEnergy);
        
        allEvents.push_back(eventTmp);

        thisEvent = std::move(eventTmp);
        data->Fill();
    }
    
    writer->cd();
    data->Write();

    std::cout << "File is closed" << std::endl;

    std::vector<std::vector<double>> results;
    
    for (auto const& record: allEvents)
    {
        results.push_back({
            record.event, record.subrun, record.run, 
            record.hitEnergy, record.ideEnergy, record.eventRatio, 
            record.p0.hitEnergy, record.p1.hitEnergy, record.p2.hitEnergy, 
            record.p0.ideEnergy, record.p1.ideEnergy, record.p2.ideEnergy, 
            record.p0.planeRatio, record.p1.planeRatio, record.p2.planeRatio, 
            record.protons, record.chargedPions, record.muons, 
            record.electrons, record.neutralPions, record.gammas
        });
    }

    return results;
}
