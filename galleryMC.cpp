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
#include "lardataobj/Simulation/SimChannel.h"
#include "sbnobj/ICARUS/TPC/ChannelROI.h"
#include "TCanvas.h"
#include <iostream>
#include <vector>
#include "TH1F.h"
#include "TH2F.h"
#include "TFile.h"
#include "TStyle.h"
#include "TROOT.h"
#include "TLegend.h"
#include <numeric>

const auto safeDivide = [](float a, float b) -> float {
    return b != 0.0f ? a / b : 0.0f;
};

float getFillValue(float hitEnergy, float ideEnergy) {
    if (ideEnergy == 0) {
        if (hitEnergy == 0) {
            return -1.0; // both zero, define ratio as -1
        } else {
            return -2.0; // hit energy non-zero, ide energy zero
        }
    } else {
        return hitEnergy / ideEnergy;
    }
}

// Function to draw wire waveform and overlay hit Gaussians
TCanvas* wireDraw(const std::vector<recob::Hit>& hits,
                 const std::vector<recob::ChannelROI>& wires,
                 int channel) {
    TCanvas* c1 = new TCanvas("c1", "Wire Waveform with Hits", 800, 600);

    // Find the wire corresponding to the specified channel
    int wireIdx = 0;
    for (size_t i = 0; i < (size_t)wires.size(); ++i) {
        if ((int)wires[i].Channel() == channel){
            wireIdx = i;
            break;
        }
    }

    TH1F* hWire = new TH1F("hWire", "Wire Waveform", wires[wireIdx].NSignal(), 0, wires[wireIdx].NSignal());  
    for (int i = 0; i < (int)wires[wireIdx].Signal().size(); ++i) {
        hWire->SetBinContent(i, wires[wireIdx].Signal()[i]);
    }  

    // Create histogram for summed Gaussians from hits
    TH1F* hHits = new TH1F("hHits", "Summed Hit Gaussians", wires[wireIdx].NSignal(), 0, wires[wireIdx].NSignal());
    
    // Filter hits for the specified channel and sum their Gaussians
    int nHitsOnChannel = 0;
    for (const auto& hit : hits) {
        if ((int)hit.Channel() == channel) {
            nHitsOnChannel++;
            float mean = hit.PeakTime();
            float amplitude = hit.PeakAmplitude();
            float sigma = hit.RMS();
            
            // Add Gaussian contribution to each bin
            for (int bin = 1; bin <= hHits->GetNbinsX(); ++bin) {
                float x = hHits->GetBinCenter(bin);
                float gaussian = amplitude * exp(-0.5 * pow((x - mean) / sigma, 2));
                hHits->SetBinContent(bin, hHits->GetBinContent(bin) + gaussian);
            }
        }
    }
    
    float maxY = std::max(hWire->GetMaximum(), hHits->GetMaximum());
    hWire->SetTitle(Form("Wire vs Hits on Channel %d", channel));
    hWire->GetYaxis()->SetTitle("ADC Counts");
    hWire->GetXaxis()->SetTitle("Time Tick");
    hWire->SetMaximum(maxY * 1.2);
    c1->cd();
    hWire->Draw();
    hHits->SetLineColor(kRed);
    hHits->Draw("same");

    TLegend* legend = new TLegend(0.6, 0.7, 0.88, 0.88);
    legend->AddEntry(hWire, "Wire ROI", "l");
    legend->AddEntry(hHits, "Hit Gaussians", "l");
    legend->Draw();

    return c1;
}

// Return the plane number given a channel ID
// From ChannelMapICARUS_20240318.db
int getPlane(int channelID){
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

std::vector<std::vector<float>> galleryMC(std::string const& inputFile = "nominalTest.root", std::string const& outputFile = "histnominalTest.root") {
    gStyle->SetOptStat(0);
    gROOT->SetBatch(kTRUE);

    int planes = 3;

    std::vector<std::string> filenames;
    filenames.push_back(inputFile);

    // Create gallery event loop
    gallery::Event ev(filenames);

    int totalEntries = ev.numberOfEventsInFile();
    std::cout << "Total number of events in file: " << totalEntries << std::endl;

    TFile* outFile = TFile::Open(outputFile.c_str(), "RECREATE");

    std::vector<TH1F*> h_hitEnergy = {};
    std::vector<TH1F*> h_ideEnergy = {};
    std::vector<TH1F*> h_energyRatio = {};
    std::vector<TH1F*> h_hitIntegral = {};
    std::vector<TH1F*> h_hitADC = {};
    std::vector<TH1F*> h_hitAreaRatio = {};
    std::vector<TH1F*> h_hitFit = {};
    std::vector<TH1F*> h_hitEnergy_ele = {};
    std::vector<TH1F*> h_ideEnergy_ele = {};
    std::vector<TH1F*> h_energyRatio_ele = {};
    std::vector<TH1F*> h_hitIntegral_ele = {};
    std::vector<TH1F*> h_hitADC_ele = {};
    std::vector<TH1F*> h_hitAreaRatio_ele = {};
    std::vector<TH1F*> h_hitFit_ele = {};
    std::vector<TH1F*> h_hitEnergy_gamma = {};
    std::vector<TH1F*> h_ideEnergy_gamma = {};
    std::vector<TH1F*> h_energyRatio_gamma = {};
    std::vector<TH1F*> h_hitIntegral_gamma = {};
    std::vector<TH1F*> h_hitADC_gamma = {};
    std::vector<TH1F*> h_hitAreaRatio_gamma = {};
    std::vector<TH1F*> h_hitFit_gamma = {};
    std::vector<TH1F*> h_hitEnergy_mu = {};
    std::vector<TH1F*> h_ideEnergy_mu = {};
    std::vector<TH1F*> h_energyRatio_mu = {};
    std::vector<TH1F*> h_hitIntegral_mu = {};
    std::vector<TH1F*> h_hitADC_mu = {};
    std::vector<TH1F*> h_hitAreaRatio_mu = {};
    std::vector<TH1F*> h_hitFit_mu = {};
    std::vector<TH1F*> h_hitEnergy_p = {};
    std::vector<TH1F*> h_ideEnergy_p = {};
    std::vector<TH1F*> h_energyRatio_p = {};
    std::vector<TH1F*> h_hitIntegral_p = {};
    std::vector<TH1F*> h_hitADC_p = {};
    std::vector<TH1F*> h_hitAreaRatio_p = {};
    std::vector<TH1F*> h_hitFit_p = {};
    std::vector<TH1F*> h_hitEnergy_pi = {};
    std::vector<TH1F*> h_ideEnergy_pi = {};
    std::vector<TH1F*> h_energyRatio_pi = {};
    std::vector<TH1F*> h_hitIntegral_pi = {};
    std::vector<TH1F*> h_hitADC_pi = {};
    std::vector<TH1F*> h_hitAreaRatio_pi = {};
    std::vector<TH1F*> h_hitFit_pi = {};
    for (int i = 0; i < planes; ++i){
        h_hitEnergy.push_back(new TH1F(Form("h_hitEnergy_plane%d", i), Form("Hit Energy from BackTrackerHitMatchingData Plane %d;Energy (MeV);Counts", i), 100, 0, 1e4));
        h_ideEnergy.push_back(new TH1F(Form("h_ideEnergy_plane%d", i), Form("IDE Energy from SimChannel Plane %d;Energy (MeV);Counts", i), 100, 0, 1e4));
        h_energyRatio.push_back(new TH1F(Form("h_energyRatio_plane%d", i), Form("Ratio of Hit Energy to IDE Energy Plane %d;Hit Energy / IDE Energy;Counts", i), 256, -2, 1.2));
        h_hitIntegral.push_back(new TH1F(Form("h_hitIntegral_plane%d", i), Form("Hit Integral all Hits Plane %d;Integral (tick x ADC);Counts", i), 100, 0, 5e3));
        h_hitADC.push_back(new TH1F(Form("h_hitADC_plane%d", i), Form("Hit Summed ADC all Hits Plane %d;Summed ADC;Counts", i), 100, 0, 5e3));
        h_hitAreaRatio.push_back(new TH1F(Form("h_hitAreaRatio_plane%d", i), Form("Hit Integral/ADC all Hits Plane %d;Hit Integral/ADC Ratio;Counts", i), 100, 0, 2));
        h_hitFit.push_back(new TH1F(Form("h_hitFit_plane%d", i), Form("Chi2/NDOF all Hits Plane %d;Chi2/NDOF;Counts", i), 100, 0, 1));
        h_hitEnergy_ele.push_back(new TH1F(Form("h_hitEnergy_ele_plane%d", i), Form("Hit Energy from BackTrackerHitMatchingData (Electron in Event) Plane %d;Energy (MeV);Counts", i), 100, 0, 1e4));
        h_ideEnergy_ele.push_back(new TH1F(Form("h_ideEnergy_ele_plane%d", i), Form("IDE Energy from SimChannel (Electron in Event) Plane %d;Energy (MeV);Counts", i), 100, 0, 1e4));
        h_energyRatio_ele.push_back(new TH1F(Form("h_energyRatio_ele_plane%d", i), Form("Ratio of Hit Energy to IDE Energy (Electron in Event) Plane %d;Hit Energy / IDE Energy;Counts", i), 256, -2, 1.2));
        h_hitIntegral_ele.push_back(new TH1F(Form("h_hitIntegral_ele_plane%d", i), Form("Hit Integral Hits from Electrons Plane %d;Integral (tick x ADC);Counts", i), 100, 0, 5e3));
        h_hitADC_ele.push_back(new TH1F(Form("h_hitADC_ele_plane%d", i), Form("Hit Summed ADC Hits from Electrons Plane %d;Summed ADC;Counts", i), 100, 0, 5e3));
        h_hitAreaRatio_ele.push_back(new TH1F(Form("h_hitAreaRatio_ele_plane%d", i), Form("Hit Integral/ADC Hits from Electrons Plane %d;Hit Integral/ADC Ratio;Counts", i), 100, 0, 2));
        h_hitFit_ele.push_back(new TH1F(Form("h_hitFit_ele_plane%d", i), Form("Chi2/NDOF Hits from Electrons Plane %d;Chi2/NDOF;Counts", i), 100, 0, 1));
        h_hitEnergy_gamma.push_back(new TH1F(Form("h_hitEnergy_gamma_plane%d", i), Form("Hit Energy from BackTrackerHitMatchingData (Photon in Event) Plane %d;Energy (MeV);Counts", i), 100, 0, 1e4));
        h_ideEnergy_gamma.push_back(new TH1F(Form("h_ideEnergy_gamma_plane%d", i), Form("IDE Energy from SimChannel (Photon in Event) Plane %d;Energy (MeV);Counts", i), 100, 0, 1e4));
        h_energyRatio_gamma.push_back(new TH1F(Form("h_energyRatio_gamma_plane%d", i), Form("Ratio of Hit Energy to IDE Energy (Photon in Event) Plane %d;Hit Energy / IDE Energy;Counts", i), 256, -2, 1.2));
        h_hitIntegral_gamma.push_back(new TH1F(Form("h_hitIntegral_gamma_plane%d", i), Form("Hit Integral Hits from Photons Plane %d;Integral (tick x ADC);Counts", i), 100, 0, 5e3));
        h_hitADC_gamma.push_back(new TH1F(Form("h_hitADC_gamma_plane%d", i), Form("Hit Summed ADC Hits from Photons Plane %d;Summed ADC;Counts", i), 100, 0, 5e3));
        h_hitAreaRatio_gamma.push_back(new TH1F(Form("h_hitAreaRatio_gamma_plane%d", i), Form("Hit Integral/ADC Hits from Photons Plane %d;Hit Integral/ADC Ratio;Counts", i), 100, 0, 2));
        h_hitFit_gamma.push_back(new TH1F(Form("h_hitFit_gamma_plane%d", i), Form("Chi2/NDOF Hits from Photons Plane %d;Chi2/NDOF;Counts", i), 100, 0, 1));
        h_hitEnergy_mu.push_back(new TH1F(Form("h_hitEnergy_mu_plane%d", i), Form("Hit Energy from BackTrackerHitMatchingData (Muon in Event) Plane %d;Energy (MeV);Counts", i), 100, 0, 1e4));
        h_ideEnergy_mu.push_back(new TH1F(Form("h_ideEnergy_mu_plane%d", i), Form("IDE Energy from SimChannel (Muon in Event) Plane %d;Energy (MeV);Counts", i), 100, 0, 1e4));
        h_energyRatio_mu.push_back(new TH1F(Form("h_energyRatio_mu_plane%d", i), Form("Ratio of Hit Energy to IDE Energy (Muon in Event) Plane %d;Hit Energy / IDE Energy;Counts", i), 256, -2, 1.2));
        h_hitIntegral_mu.push_back(new TH1F(Form("h_hitIntegral_mu_plane%d", i), Form("Hit Integral Hits from Muons Plane %d;Integral (tick x ADC);Counts", i), 100, 0, 5e3));
        h_hitADC_mu.push_back(new TH1F(Form("h_hitADC_mu_plane%d", i), Form("Hit Summed ADC Hits from Muons Plane %d;Summed ADC;Counts", i), 100, 0, 5e3));
        h_hitAreaRatio_mu.push_back(new TH1F(Form("h_hitAreaRatio_mu_plane%d", i), Form("Hit Integral/ADC Hits from Muons Plane %d;Hit Integral/ADC Ratio;Counts", i), 100, 0, 2));
        h_hitFit_mu.push_back(new TH1F(Form("h_hitFit_mu_plane%d", i), Form("Chi2/NDOF Hits from Muons Plane %d;Chi2/NDOF;Counts", i), 100, 0, 1));
        h_hitEnergy_p.push_back(new TH1F(Form("h_hitEnergy_p_plane%d", i), Form("Hit Energy from BackTrackerHitMatchingData (Proton in Event) Plane %d;Energy (MeV);Counts", i), 100, 0, 1e4));
        h_ideEnergy_p.push_back(new TH1F(Form("h_ideEnergy_p_plane%d", i), Form("IDE Energy from SimChannel (Proton in Event) Plane %d;Energy (MeV);Counts", i), 100, 0, 1e4));
        h_energyRatio_p.push_back(new TH1F(Form("h_energyRatio_p_plane%d", i), Form("Ratio of Hit Energy to IDE Energy (Proton in Event) Plane %d;Hit Energy / IDE Energy;Counts", i), 256, -2, 1.2));
        h_hitIntegral_p.push_back(new TH1F(Form("h_hitIntegral_p_plane%d", i), Form("Hit Integral Hits from Protons Plane %d;Integral (tick x ADC);Counts", i), 100, 0, 5e3));
        h_hitADC_p.push_back(new TH1F(Form("h_hitADC_p_plane%d", i), Form("Hit Summed ADC Hits from Protons Plane %d;Summed ADC;Counts", i), 100, 0, 5e3));
        h_hitAreaRatio_p.push_back(new TH1F(Form("h_hitAreaRatio_p_plane%d", i), Form("Hit Integral/ADC Hits from Protons Plane %d;Hit Integral/ADC Ratio;Counts", i), 100, 0, 2));
        h_hitFit_p.push_back(new TH1F(Form("h_hitFit_p_plane%d", i), Form("Chi2/NDOF Hits from Protons Plane %d;Chi2/NDOF;Counts", i), 100, 0, 1));
        h_hitEnergy_pi.push_back(new TH1F(Form("h_hitEnergy_pi_plane%d", i), Form("Hit Energy from BackTrackerHitMatchingData (Pion in Event) Plane %d;Energy (MeV);Counts", i), 100, 0, 1e4));
        h_ideEnergy_pi.push_back(new TH1F(Form("h_ideEnergy_pi_plane%d", i), Form("IDE Energy from SimChannel (Pion in Event) Plane %d;Energy (MeV);Counts", i), 100, 0, 1e4));
        h_energyRatio_pi.push_back(new TH1F(Form("h_energyRatio_pi_plane%d", i), Form("Ratio of Hit Energy to IDE Energy (Pion in Event) Plane %d;Hit Energy / IDE Energy;Counts", i), 256, -2, 1.2));
        h_hitIntegral_pi.push_back(new TH1F(Form("h_hitIntegral_pi_plane%d", i), Form("Hit Integral Hits from Pions Plane %d;Integral (tick x ADC);Counts", i), 100, 0, 5e3));
        h_hitADC_pi.push_back(new TH1F(Form("h_hitADC_pi_plane%d", i), Form("Hit Summed ADC Hits from Pions Plane %d;Summed ADC;Counts", i), 100, 0, 5e3));
        h_hitAreaRatio_pi.push_back(new TH1F(Form("h_hitAreaRatio_pi_plane%d", i), Form("Hit Integral/ADC Hits from Pions Plane %d;Hit Integral/ADC Ratio;Counts", i), 100, 0, 2));
        h_hitFit_pi.push_back(new TH1F(Form("h_hitFit_pi_plane%d", i), Form("Chi2/NDOF Hits from Pions Plane %d;Chi2/NDOF;Counts", i), 100, 0, 1));
    }

    TH1F* h_maxETheta = new TH1F("h_maxETheta", "Theta of Highest Energy Particle per Event;Theta (radians);Counts", 100, -4, 4);
    TH1F* h_maxEPhi = new TH1F("h_maxEPhi", "Phi of Highest Energy Particle per Event;Phi (radians);Counts", 100, -4, 4);
    TH2F* h_maxETheta_vs_E = new TH2F("h_maxETheta_vs_E", "Theta vs Energy of Highest Energy Particle per Event;Theta (radians);Energy (MeV)", 100, -4, 4, 256, -2, 1.2);
    TH2F* h_maxEPhi_vs_E = new TH2F("h_maxEPhi_vs_E", "Phi vs Energy of Highest Energy Particle per Event;Phi (radians);Energy (MeV)", 100, -4, 4, 256, -2, 1.2);
    TH1F* h_maxETheta_ele = new TH1F("h_maxETheta_ele", "Theta of Highest Energy Electron per Event;Theta (radians);Counts", 100, -4, 4);
    TH1F* h_maxEPhi_ele = new TH1F("h_maxEPhi_ele", "Phi of Highest Energy Electron per Event;Phi (radians);Counts", 100, -4, 4);
    TH2F* h_maxETheta_vs_E_ele = new TH2F("h_maxETheta_vs_E_ele", "Theta vs Energy of Highest Energy Electron per Event;Theta (radians);Energy (MeV)", 100, -4, 4, 256, -2, 1.2);
    TH2F* h_maxEPhi_vs_E_ele = new TH2F("h_maxEPhi_vs_E_ele", "Phi vs Energy of Highest Energy Electron per Event;Phi (radians);Energy (MeV)", 100, -4, 4, 256, -2, 1.2);
    TH1F* h_maxETheta_gamma = new TH1F("h_maxETheta_gamma", "Theta of Highest Energy Photon per Event;Theta (radians);Counts", 100, -4, 4);
    TH1F* h_maxEPhi_gamma = new TH1F("h_maxEPhi_gamma", "Phi of Highest Energy Photon per Event;Phi (radians);Counts", 100, -4, 4);
    TH2F* h_maxETheta_vs_E_gamma = new TH2F("h_maxETheta_vs_E_gamma", "Theta vs Energy of Highest Energy Photon per Event;Theta (radians);Energy (MeV)", 100, -4, 4, 256, -2, 1.2);
    TH2F* h_maxEPhi_vs_E_gamma = new TH2F("h_maxEPhi_vs_E_gamma", "Phi vs Energy of Highest Energy Photon per Event;Phi (radians);Energy (MeV)", 100, -4, 4, 256, -2, 1.2);
    TH1F* h_maxETheta_mu = new TH1F("h_maxETheta_mu", "Theta of Highest Energy Muon per Event;Theta (radians);Counts", 100, -4, 4);
    TH1F* h_maxEPhi_mu = new TH1F("h_maxEPhi_mu", "Phi of Highest Energy Muon per Event;Phi (radians);Counts", 100, -4, 4);
    TH2F* h_maxETheta_vs_E_mu = new TH2F("h_maxETheta_vs_E_mu", "Theta vs Energy of Highest Energy Muon per Event;Theta (radians);Energy (MeV)", 100, -4, 4, 256, -2, 1.2);
    TH2F* h_maxEPhi_vs_E_mu = new TH2F("h_maxEPhi_vs_E_mu", "Phi vs Energy of Highest Energy Muon per Event;Phi (radians);Energy (MeV)", 100, -4, 4, 256, -2, 1.2);
    TH1F* h_maxETheta_p = new TH1F("h_maxETheta_p", "Theta of Highest Energy Proton per Event;Theta (radians);Counts", 100, -4, 4);
    TH1F* h_maxEPhi_p = new TH1F("h_maxEPhi_p", "Phi of Highest Energy Proton per Event;Phi (radians);Counts", 100, -4, 4);
    TH2F* h_maxETheta_vs_E_p = new TH2F("h_maxETheta_vs_E_p", "Theta vs Energy of Highest Energy Proton per Event;Theta (radians);Energy (MeV)", 100, -4, 4, 256, -2, 1.2);
    TH2F* h_maxEPhi_vs_E_p = new TH2F("h_maxEPhi_vs_E_p", "Phi vs Energy of Highest Energy Proton per Event;Phi (radians);Energy (MeV)", 100, -4, 4, 256, -2, 1.2);
    TH1F* h_maxETheta_pi = new TH1F("h_maxETheta_pi", "Theta of Highest Energy Pion per Event;Theta (radians);Counts", 100, -4, 4);
    TH1F* h_maxEPhi_pi = new TH1F("h_maxEPhi_pi", "Phi of Highest Energy Pion per Event;Phi (radians);Counts", 100, -4, 4);
    TH2F* h_maxETheta_vs_E_pi = new TH2F("h_maxETheta_vs_E_pi", "Theta vs Energy of Highest Energy Pion per Event;Theta (radians);Energy (MeV)", 100, -4, 4, 256, -2, 1.2);
    TH2F* h_maxEPhi_vs_E_pi = new TH2F("h_maxEPhi_vs_E_pi", "Phi vs Energy of Highest Energy Pion per Event;Phi (radians);Energy (MeV)", 100, -4, 4, 256, -2, 1.2);

    TH1F* h_particleCount = new TH1F("h_particleCount", "Particle Count per Event;Particle Type;Counts", 6, 0, 6);
    TH1F* h_maxEParticleCount = new TH1F("h_maxEParticleCount", "Highest Energy Particle per Event;Particle Type;Counts", 6, 0, 6);

    std::vector<TString> planeLabels = {"WW", "WE", "EW", "EE"};
    std::vector<TH1F*> hPeakAmplitude = {};
    std::vector<TH1F*> hNHits = {};
    std::vector<TH1F*> hRMS = {};
    std::vector<TH1F*> hIntegral = {};
    std::vector<TH1F*> hGoodnessOfFit = {};
    std::vector<TH1F*> hHitSummedADC = {};
    std::vector<TH1F*> hROISummedADC = {};
    std::vector<TH1F*> hChannel = {};
    for ( auto& id : planeLabels){
        hPeakAmplitude.push_back(new TH1F(Form("hPeakAmplitude_%s", id.Data()), Form("Hit Peak Amplitude %s;Amplitude;Counts", id.Data()), 400, 0, 400));
        hNHits.push_back(new TH1F(Form("hNHits_%s", id.Data()), Form("Number of Hits %s;Number of Hits;Counts", id.Data()), 250, 0, 1000));
        hRMS.push_back(new TH1F(Form("hRMS_%s", id.Data()), Form("Hit RMS %s;RMS;Counts", id.Data()), 100, 0, 20));
        hIntegral.push_back(new TH1F(Form("hIntegral_%s", id.Data()), Form("Hit Integral %s;Integral;Counts", id.Data()), 500, 0, 2000));
        hGoodnessOfFit.push_back(new TH1F(Form("hGoodnessOfFit_%s", id.Data()), Form("Hit Goodness of Fit %s;Goodness of Fit;Counts", id.Data()), 50, 0, 10));
        hHitSummedADC.push_back(new TH1F(Form("hHitSummedADC_%s", id.Data()), Form("Hit Summed ADC %s;Hit Summed ADC;Counts", id.Data()), 500, 0, 2000));
        hROISummedADC.push_back(new TH1F(Form("hROISummedADC_%s", id.Data()), Form("ROI Summed ADC %s;ROI Summed ADC;Counts", id.Data()), 500, 0, 2000));
        hChannel.push_back(new TH1F(Form("hChannel_%s", id.Data()), Form("Hit Channel %s;Channel;Counts", id.Data()), 3500, 0, 3500));
    }

    TCanvas* c_wire = nullptr;

    float totalHitEnergy[3] = {0.0, 0.0, 0.0};
    float totalIdeEnergy[3] = {0.0, 0.0, 0.0};
    float totalHitEnergyEle[3] = {0.0, 0.0, 0.0};
    float totalIdeEnergyEle[3] = {0.0, 0.0, 0.0};
    float totalHitEnergyGamma[3] = {0.0, 0.0, 0.0};
    float totalIdeEnergyGamma[3] = {0.0, 0.0, 0.0};
    float totalHitEnergyMu[3] = {0.0, 0.0, 0.0};
    float totalIdeEnergyMu[3] = {0.0, 0.0, 0.0};
    float totalHitEnergyP[3] = {0.0, 0.0, 0.0};
    float totalIdeEnergyP[3] = {0.0, 0.0, 0.0};
    float totalHitEnergyPi[3] = {0.0, 0.0, 0.0};
    float totalIdeEnergyPi[3] = {0.0, 0.0, 0.0};

    int evtCounter = 0;
    // Loop over events
    while (!ev.atEnd()) {
        float eventHitEnergy[3] = {0.0, 0.0, 0.0};
        float eventIdeEnergy[3] = {0.0, 0.0, 0.0};
        bool foundElectron = false;
        bool foundPhoton = false;
        bool foundMuon = false;
        bool foundPion = false;
        bool foundProton = false;

        auto const& hitTruthAssnsHandle = ev.getValidHandle<art::Assns<recob::Hit, simb::MCParticle, anab::BackTrackerHitMatchingData>>("mcassociationsGausCryoE");
        auto const& mcHandle = *ev.getValidHandle<std::vector<simb::MCParticle>>("largeant");
        auto const& simHandle = *ev.getValidHandle<std::vector<sim::SimChannel>>("merge");
        auto const& wireEE =
           *ev.getValidHandle<std::vector<recob::ChannelROI>>({"wire2channelroi2d", "PHYSCRATEDATATPCEE"});

        auto const& hitTruthAssns = *hitTruthAssnsHandle;  // Dereference handle
        
        auto const& hitsWW =
           *ev.getValidHandle<std::vector<recob::Hit>>({"gaushit2dTPCWW"});
        auto const& hitsWE =
           *ev.getValidHandle<std::vector<recob::Hit>>({"gaushit2dTPCWE"});
        auto const& hitsEW =
           *ev.getValidHandle<std::vector<recob::Hit>>({"gaushit2dTPCEW"});
        auto const& hitsEE =
           *ev.getValidHandle<std::vector<recob::Hit>>({"gaushit2dTPCEE"});

        std::vector< const std::vector<recob::Hit>* > allHits = {&hitsWW, &hitsWE, &hitsEW, &hitsEE};
        
        for (size_t i = 0; i < allHits.size(); ++i) {
            const auto& hits = *allHits[i];
            for (const auto& hit : hits) {
                hPeakAmplitude[i]->Fill(hit.PeakAmplitude());
                hNHits[i]->Fill(hits.size());
                hRMS[i]->Fill(hit.RMS());
                hIntegral[i]->Fill(hit.Integral());
                hGoodnessOfFit[i]->Fill(hit.GoodnessOfFit());
                hHitSummedADC[i]->Fill(hit.HitSummedADC());
                hROISummedADC[i]->Fill(hit.ROISummedADC());
                hChannel[i]->Fill(hit.WireID().getIndex<3>());
            }
        }

        std::unordered_map<int, simb::MCParticle const*> trackIdToParticle;
        for (auto const& p : mcHandle) {
            trackIdToParticle[p.TrackId()] = &p;
        }

        if (ev.eventAuxiliary().event() == 17559 && ev.eventAuxiliary().run() == 9311) {
            c_wire = wireDraw(hitsEE, wireEE, 609);
        }

        for (auto it = hitTruthAssns.begin(); it != hitTruthAssns.end(); ++it) {
            auto const& [hit, mcpart, _unused] = *it; // unpack the 3 elements
            auto const& matchdata = hitTruthAssns.data(it); // Use iterator
            int plane = hit->WireID().getIndex<2>();
            if (abs(mcpart->PdgCode()) == 11){
                if(matchdata.energy > 0) foundElectron = true;
                if(matchdata.ideFraction > 0.5){
                    h_hitIntegral_ele[plane]->Fill(hit->Integral());
                    h_hitADC_ele[plane]->Fill(hit->HitSummedADC());
                    h_hitAreaRatio_ele[plane]->Fill(hit->Integral()/hit->HitSummedADC());
                    h_hitFit_ele[plane]->Fill(hit->GoodnessOfFit()/hit->DegreesOfFreedom());
                }
            }
            if (abs(mcpart->PdgCode()) == 22){
                if(matchdata.energy > 0) foundPhoton = true;
                if(matchdata.ideFraction > 0.5){
                    h_hitIntegral_gamma[plane]->Fill(hit->Integral());
                    h_hitADC_gamma[plane]->Fill(hit->HitSummedADC());
                    h_hitAreaRatio_gamma[plane]->Fill(hit->Integral()/hit->HitSummedADC());
                    h_hitFit_gamma[plane]->Fill(hit->GoodnessOfFit()/hit->DegreesOfFreedom());
                }
            }
            if (abs(mcpart->PdgCode()) == 13){
                if(matchdata.energy > 0) foundMuon = true;
                if(matchdata.ideFraction > 0.5){
                    h_hitIntegral_mu[plane]->Fill(hit->Integral());
                    h_hitADC_mu[plane]->Fill(hit->HitSummedADC());
                    h_hitAreaRatio_mu[plane]->Fill(hit->Integral()/hit->HitSummedADC());
                    h_hitFit_mu[plane]->Fill(hit->GoodnessOfFit()/hit->DegreesOfFreedom());
                }
            } 
            if (abs(mcpart->PdgCode()) == 211 || abs(mcpart->PdgCode()) == 111){
                if(matchdata.energy > 0) foundPion = true;
                if(matchdata.ideFraction > 0.5){
                    h_hitIntegral_pi[plane]->Fill(hit->Integral());
                    h_hitADC_pi[plane]->Fill(hit->HitSummedADC());
                    h_hitAreaRatio_pi[plane]->Fill(hit->Integral()/hit->HitSummedADC());
                    h_hitFit_pi[plane]->Fill(hit->GoodnessOfFit()/hit->DegreesOfFreedom());
                }
            } 
            if (abs(mcpart->PdgCode()) == 2212){
                if(matchdata.energy > 0) foundProton = true;
                if(matchdata.ideFraction > 0.5){
                    h_hitIntegral_p[plane]->Fill(hit->Integral());
                    h_hitADC_p[plane]->Fill(hit->HitSummedADC());
                    h_hitAreaRatio_p[plane]->Fill(hit->Integral()/hit->HitSummedADC());
                    h_hitFit_p[plane]->Fill(hit->GoodnessOfFit()/hit->DegreesOfFreedom());
                }
            }

            h_hitIntegral[plane]->Fill(hit->Integral());
            h_hitADC[plane]->Fill(hit->HitSummedADC());
            h_hitAreaRatio[plane]->Fill(hit->Integral()/hit->HitSummedADC());
            h_hitFit[plane]->Fill(hit->GoodnessOfFit()/hit->DegreesOfFreedom());

            eventHitEnergy[plane] += matchdata.energy * matchdata.ideFraction;

            /*if ( showerMatched ) {
                std::cout << "Event " << evtCounter << ": Matched hit on channel " << hit->Channel() << " start time " << hit->StartTick()
                          << " to MCParticle PDG " << mcpart->PdgCode() << " Track ID " << mcpart->TrackId()
                          << " with energy " << matchdata.energy << " MeV" << " with fraction " << matchdata.ideFraction
                          << " and nPE " << matchdata.numElectrons << " with fraction " << matchdata.ideNFraction << std::endl;
                sumEnergy += matchdata.energy;
            }*/

        }

        if (!foundElectron && !foundPhoton && !foundMuon && !foundPion && !foundProton) {
            ev.next();
            evtCounter++;
            continue;
        }
            
        // from mcP get track ID -> get sim channel -> get ide -> get energy
        // sim channel (mcp.TrackId) -> ide -> energy
        int maxEParticle = -9999;
        float maxE = -1.0;
        int maxTrackId = -1;
        for (auto const& sc : simHandle) {
            int plane = getPlane(sc.Channel());
            auto const& tdcide_map = sc.TDCIDEMap();
            for (auto const& kv : tdcide_map) {
                unsigned int tick = kv.first;
                for (auto const& ide : kv.second) {
                    int trackID = ide.trackID;
                    float energy = ide.energy;
                    eventIdeEnergy[plane] += energy;
                    if (ide.energy > maxE) {
                        maxE = ide.energy;
                        maxTrackId = trackID;
                        if (trackIdToParticle.find(trackID) != trackIdToParticle.end()) {
                            maxEParticle = trackIdToParticle[trackID]->PdgCode();
                        }
                    }
                }
            }
        }

        float maxE_theta = -9999.0;
        float maxE_phi = -9999.0;
        if (maxTrackId != -1 && trackIdToParticle.find(maxTrackId) != trackIdToParticle.end()) {
            auto const* maxParticle = trackIdToParticle[maxTrackId];
            float pX = maxParticle->Px();
            float pY = maxParticle->Py();
            float pZ = maxParticle->Pz();
            
            TVector3 pVec(pX, pY, pZ);
            
            maxE_theta = pVec.Theta();
            maxE_phi = pVec.Phi();
        }

        if (maxEParticle != -9999) {
            if (abs(maxEParticle) == 11) h_maxEParticleCount->Fill(0);
            else if (abs(maxEParticle) == 22) h_maxEParticleCount->Fill(1);
            else if (abs(maxEParticle) == 13) h_maxEParticleCount->Fill(2);
            else if (abs(maxEParticle) == 2212) h_maxEParticleCount->Fill(3);
            else if (abs(maxEParticle) == 211 || abs(maxEParticle) == 111) h_maxEParticleCount->Fill(4);
            else h_maxEParticleCount->Fill(5);
        }

        float totalEnergyRatio = 0.0;
        for (size_t i = 0; i < planes; ++i) {
            if (eventIdeEnergy[i] > 0) {
                totalEnergyRatio += eventHitEnergy[i] / eventIdeEnergy[i];
            }
        }

        h_maxETheta->Fill(maxE_theta);
        h_maxEPhi->Fill(maxE_phi);
        h_maxETheta_vs_E->Fill(maxE_theta, totalEnergyRatio);
        h_maxEPhi_vs_E->Fill(maxE_phi, totalEnergyRatio);

        for (size_t plane = 0; plane < planes; ++plane) {
            h_hitEnergy[plane]->Fill(eventHitEnergy[plane]);
            h_ideEnergy[plane]->Fill(eventIdeEnergy[plane]);
            float ratio = getFillValue(eventHitEnergy[plane], eventIdeEnergy[plane]);
            h_energyRatio[plane]->Fill(ratio);
            totalHitEnergy[plane] += eventHitEnergy[plane];
            totalIdeEnergy[plane] += eventIdeEnergy[plane];

            if (foundElectron) {
                h_hitEnergy_ele[plane]->Fill(eventHitEnergy[plane]);
                h_ideEnergy_ele[plane]->Fill(eventIdeEnergy[plane]);
                h_energyRatio_ele[plane]->Fill(ratio);
                h_particleCount->Fill(0);
                totalHitEnergyEle[plane] += eventHitEnergy[plane];
                totalIdeEnergyEle[plane] += eventIdeEnergy[plane];
                if(abs(maxEParticle) == 11 && plane == 0) {
                    h_maxETheta_ele->Fill(maxE_theta);
                    h_maxEPhi_ele->Fill(maxE_phi);
                    h_maxETheta_vs_E_ele->Fill(maxE_theta, totalEnergyRatio);
                    h_maxEPhi_vs_E_ele->Fill(maxE_phi, totalEnergyRatio);
                }
            }
            if (foundPhoton) {
                h_hitEnergy_gamma[plane]->Fill(eventHitEnergy[plane]);
                h_ideEnergy_gamma[plane]->Fill(eventIdeEnergy[plane]);
                h_energyRatio_gamma[plane]->Fill(ratio);
                h_particleCount->Fill(1);
                totalHitEnergyGamma[plane] += eventHitEnergy[plane];
                totalIdeEnergyGamma[plane] += eventIdeEnergy[plane];
                if(abs(maxEParticle) == 22 && plane == 0) {
                    h_maxETheta_gamma->Fill(maxE_theta);
                    h_maxEPhi_gamma->Fill(maxE_phi);
                    h_maxETheta_vs_E_gamma->Fill(maxE_theta, totalEnergyRatio);
                    h_maxEPhi_vs_E_gamma->Fill(maxE_phi, totalEnergyRatio);
                }
            }
            if (foundMuon) {
                h_hitEnergy_mu[plane]->Fill(eventHitEnergy[plane]);
                h_ideEnergy_mu[plane]->Fill(eventIdeEnergy[plane]);
                h_energyRatio_mu[plane]->Fill(ratio);
                h_particleCount->Fill(2);
                totalHitEnergyMu[plane] += eventHitEnergy[plane];
                totalIdeEnergyMu[plane] += eventIdeEnergy[plane];
                if(abs(maxEParticle) == 13 && plane == 0) {
                    h_maxETheta_mu->Fill(maxE_theta);
                    h_maxEPhi_mu->Fill(maxE_phi);
                    h_maxETheta_vs_E_mu->Fill(maxE_theta, totalEnergyRatio);
                    h_maxEPhi_vs_E_mu->Fill(maxE_phi, totalEnergyRatio);
                }
            }
            if (foundProton) {
                h_hitEnergy_p[plane]->Fill(eventHitEnergy[plane]);
                h_ideEnergy_p[plane]->Fill(eventIdeEnergy[plane]);
                h_energyRatio_p[plane]->Fill(ratio);
                h_particleCount->Fill(3);
                totalHitEnergyP[plane] += eventHitEnergy[plane];
                totalIdeEnergyP[plane] += eventIdeEnergy[plane];
                if(abs(maxEParticle) == 2212 && plane == 0) {
                    h_maxETheta_ele->Fill(maxE_theta);
                    h_maxEPhi_ele->Fill(maxE_phi);
                    h_maxETheta_vs_E->Fill(maxE_theta, totalEnergyRatio);
                    h_maxEPhi_vs_E->Fill(maxE_phi, totalEnergyRatio);
                }
            }
            if (foundPion) {
                h_hitEnergy_pi[plane]->Fill(eventHitEnergy[plane]);
                h_ideEnergy_pi[plane]->Fill(eventIdeEnergy[plane]);
                h_energyRatio_pi[plane]->Fill(ratio);
                h_particleCount->Fill(4);
                totalHitEnergyPi[plane] += eventHitEnergy[plane];
                totalIdeEnergyPi[plane] += eventIdeEnergy[plane];
                if((abs(maxEParticle) == 211 || abs(maxEParticle) == 111) && plane == 0) {
                    h_maxETheta_pi->Fill(maxE_theta);
                    h_maxEPhi_pi->Fill(maxE_phi);
                    h_maxETheta_vs_E_pi->Fill(maxE_theta, totalEnergyRatio);
                    h_maxEPhi_vs_E_pi->Fill(maxE_phi, totalEnergyRatio);
                }
            }
        }

        ev.next();
        evtCounter++;

    } // end of event loop

    for (int i = 0; i < planes; ++i) {
        std::cout << "Plane " << i << " total Hit Energy over all events: " << totalHitEnergy[i] << " MeV" << std::endl;
        std::cout << "Plane " << i << " total IDE Energy over all events: " << totalIdeEnergy[i] << " MeV" << std::endl;
    }

    outFile->cd();
    h_particleCount->Write();
    h_maxEParticleCount->Write();

    outFile->mkdir("AllParticles");
    outFile->cd("AllParticles");
    for (int i = 0; i < planes; ++i) {
        h_hitEnergy[i]->Write();
        h_ideEnergy[i]->Write();
        h_energyRatio[i]->Write();
        h_hitIntegral[i]->Write();
        h_hitADC[i]->Write();
        h_hitAreaRatio[i]->Write();
        h_hitFit[i]->Write();
    }
    h_maxETheta->Write();
    h_maxEPhi->Write();
    h_maxETheta_vs_E->Write();
    h_maxEPhi_vs_E->Write();

    outFile->mkdir("Electrons");
    outFile->cd("Electrons");
    for (int i = 0; i < planes; ++i) {
        h_hitEnergy_ele[i]->Write();
        h_ideEnergy_ele[i]->Write();
        h_energyRatio_ele[i]->Write();
        h_hitIntegral_ele[i]->Write();
        h_hitADC_ele[i]->Write();
        h_hitAreaRatio_ele[i]->Write();
        h_hitFit_ele[i]->Write();
    }
    h_maxETheta_ele->Write();
    h_maxEPhi_ele->Write();
    h_maxETheta_vs_E_ele->Write();
    h_maxEPhi_vs_E_ele->Write();

    outFile->cd();
    outFile->mkdir("Photons");
    outFile->cd("Photons");
    for (int i = 0; i < planes; ++i) {
        h_hitEnergy_gamma[i]->Write();
        h_ideEnergy_gamma[i]->Write();
        h_energyRatio_gamma[i]->Write();
        h_hitIntegral_gamma[i]->Write();
        h_hitADC_gamma[i]->Write();
        h_hitAreaRatio_gamma[i]->Write();
        h_hitFit_gamma[i]->Write();
    }
    h_maxETheta_gamma->Write();
    h_maxEPhi_gamma->Write();
    h_maxETheta_vs_E_gamma->Write();
    h_maxEPhi_vs_E_gamma->Write();
    outFile->cd();

    outFile->mkdir("Muons");
    outFile->cd("Muons");
    for (int i = 0; i < planes; ++i) {
        h_hitEnergy_mu[i]->Write();
        h_ideEnergy_mu[i]->Write();
        h_energyRatio_mu[i]->Write();
        h_hitIntegral_mu[i]->Write();
        h_hitADC_mu[i]->Write();
        h_hitAreaRatio_mu[i]->Write();
        h_hitFit_mu[i]->Write();
    }
    h_maxETheta_mu->Write();
    h_maxEPhi_mu->Write();    
    h_maxETheta_vs_E_mu->Write();
    h_maxEPhi_vs_E_mu->Write();
    outFile->cd();

    outFile->mkdir("Protons");
    outFile->cd("Protons");
    for (int i = 0; i < planes; ++i) {
        h_hitEnergy_p[i]->Write();
        h_ideEnergy_p[i]->Write();
        h_energyRatio_p[i]->Write();
        h_hitIntegral_p[i]->Write();
        h_hitADC_p[i]->Write();
        h_hitAreaRatio_p[i]->Write();
        h_hitFit_p[i]->Write();
    }
    h_maxETheta_p->Write();
    h_maxEPhi_p->Write();    
    h_maxETheta_vs_E_p->Write();
    h_maxEPhi_vs_E_p->Write();
    outFile->cd();

    outFile->mkdir("Pions");
    outFile->cd("Pions");
    for (int i = 0; i < planes; ++i) {
        h_hitEnergy_pi[i]->Write();
        h_ideEnergy_pi[i]->Write();
        h_energyRatio_pi[i]->Write();
        h_hitIntegral_pi[i]->Write();
        h_hitADC_pi[i]->Write();
        h_hitAreaRatio_pi[i]->Write();
        h_hitFit_pi[i]->Write();
    }
    h_maxETheta_pi->Write();
    h_maxEPhi_pi->Write();    
    h_maxETheta_vs_E_pi->Write();
    h_maxEPhi_vs_E_pi->Write();
    outFile->cd();

    for (int i = 0; i < planeLabels.size(); ++i) {
        outFile->mkdir("Hits_" + planeLabels[i]);
        outFile->cd("Hits_" + planeLabels[i]);
        hPeakAmplitude[i]->Write();
        hNHits[i]->Write();
        hRMS[i]->Write();
        hIntegral[i]->Write();
        hGoodnessOfFit[i]->Write();
        hHitSummedADC[i]->Write();
        hROISummedADC[i]->Write();
        hChannel[i]->Write();
    }
    outFile->cd();
    if(c_wire) c_wire->Write("WireWaveformWithHits");

    outFile->Close();

    std::vector<std::vector<float>> results;
    results.push_back(std::vector<float>{
                                        safeDivide(std::accumulate(totalHitEnergy, totalHitEnergy+3, 0), std::accumulate(totalIdeEnergy, totalIdeEnergy+3, 0.0)),
                                        safeDivide(totalHitEnergy[0], totalIdeEnergy[0]),
                                        safeDivide(totalHitEnergy[1], totalIdeEnergy[1]),
                                        safeDivide(totalHitEnergy[2], totalIdeEnergy[2])});
    results.push_back(std::vector<float>{
                                        safeDivide(std::accumulate(totalHitEnergyEle, totalHitEnergyEle+3, 0), std::accumulate(totalIdeEnergyEle, totalIdeEnergyEle+3, 0.0)),
                                        safeDivide(totalHitEnergyEle[0], totalIdeEnergyEle[0]),
                                        safeDivide(totalHitEnergyEle[1], totalIdeEnergyEle[1]),
                                        safeDivide(totalHitEnergyEle[2], totalIdeEnergyEle[2])});
    results.push_back(std::vector<float>{
                                        safeDivide(std::accumulate(totalHitEnergyGamma, totalHitEnergyGamma+3, 0), std::accumulate(totalIdeEnergyGamma, totalIdeEnergyGamma+3, 0.0)),
                                        safeDivide(totalHitEnergyGamma[0], totalIdeEnergyGamma[0]),
                                        safeDivide(totalHitEnergyGamma[1], totalIdeEnergyGamma[1]),
                                        safeDivide(totalHitEnergyGamma[2], totalIdeEnergyGamma[2])});
    results.push_back(std::vector<float>{
                                        safeDivide(std::accumulate(totalHitEnergyMu, totalHitEnergyMu+3, 0), std::accumulate(totalIdeEnergyMu, totalIdeEnergyMu+3, 0.0)),
                                        safeDivide(totalHitEnergyMu[0], totalIdeEnergyMu[0]),
                                        safeDivide(totalHitEnergyMu[1], totalIdeEnergyMu[1]),
                                        safeDivide(totalHitEnergyMu[2], totalIdeEnergyMu[2])});
    results.push_back(std::vector<float>{
                                        safeDivide(std::accumulate(totalHitEnergyP, totalHitEnergyP+3, 0), std::accumulate(totalIdeEnergyP, totalIdeEnergyP+3, 0.0)),
                                        safeDivide(totalHitEnergyP[0], totalIdeEnergyP[0]),
                                        safeDivide(totalHitEnergyP[1], totalIdeEnergyP[1]),
                                        safeDivide(totalHitEnergyP[2], totalIdeEnergyP[2])});
    results.push_back(std::vector<float>{
                                        safeDivide(std::accumulate(totalHitEnergyPi, totalHitEnergyPi+3, 0), std::accumulate(totalIdeEnergyPi, totalIdeEnergyPi+3, 0.0)),
                                        safeDivide(totalHitEnergyPi[0], totalIdeEnergyPi[0]),
                                        safeDivide(totalHitEnergyPi[1], totalIdeEnergyPi[1]),
                                        safeDivide(totalHitEnergyPi[2], totalIdeEnergyPi[2])});
    
    return results;
}