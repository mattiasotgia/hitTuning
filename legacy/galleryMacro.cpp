#include "gallery/Event.h"
#include "canvas/Persistency/Common/Wrapper.h"
#include "lardataobj/RecoBase/Hit.h"
#include "lardataobj/RecoBase/Wire.h"
#include "sbnobj/ICARUS/TPC/ChannelROI.h"
#include "TCanvas.h"
#include <iostream>
#include <vector>
#include "TH1F.h"
#include "TFile.h"
#include "TStyle.h"
#include "TROOT.h"
#include "TLegend.h"

// Function to draw wire waveform and overlay hit Gaussians
TCanvas* wireDraw(const std::vector<recob::Hit>& hits,
                 const std::vector<recob::ChannelROI>& wires,
                 int channel,
                 int timeLow, 
                 int timeHigh) {
    TCanvas* c1 = new TCanvas("c1", "Wire Waveform with Hits", 800, 600);

    // Find the wire corresponding to the specified channel
    int wireIdx = 0;
    for (size_t i = 0; i < (size_t)wires.size(); ++i) {
        if ((int)wires[i].Channel() == channel){
            std::cout << "Wire " << i << " has " << wires[i].NSignal() << " samples." << std::endl;
            wireIdx = i;
            break;
        }
    }

    std::cout << "Drawing wire index: " << wireIdx << " channel: " << wires[wireIdx].Channel() << std::endl;
    TH1F* hWire = new TH1F("hWire", "Wire Waveform", timeHigh-timeLow, timeLow, timeHigh);  

    auto adcs = wires[wireIdx].SignalROIF();
    for (int i = 0; i < (int)adcs.size(); ++i) {
        hWire->SetBinContent(i, adcs[i] / wires[wireIdx].ADCScaleFactor());
    }  

    // Create histogram for summed Gaussians from hits
    TH1F* hHits = new TH1F("hHits", "Summed Hit Gaussians", timeHigh-timeLow, timeLow, timeHigh);
    
    // Filter hits for the specified channel and sum their Gaussians
    int nHitsOnChannel = 0;
    std::cout << "Total of " << hits.size() << " hits in the event." << std::endl;
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
    std::cout << "Found " << nHitsOnChannel << " hits on channel " << channel << std::endl;
    std::cout << "Max hit amplitude: " << hHits->GetMaximum() << std::endl;
    
    float maxY = std::max(hWire->GetMaximum(), hHits->GetMaximum());
    hWire->SetTitle(Form("Wire vs Hits on Channel %d", channel));
    hWire->GetYaxis()->SetTitle("ADC Counts");
    hWire->GetXaxis()->SetTitle("Time Tick");
    hWire->SetMaximum(maxY * 1.2);
    hWire->GetXaxis()->SetRangeUser(timeLow, timeHigh);
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

void galleryMacro(std::string const& inputFile = "nominalTest.root", std::string const& outputFile = "histnominalTest.root", int channel = 15700, int timeLow = 0, int timeHigh = 5000) {

    gStyle->SetOptStat(0);
    gROOT->SetBatch(kTRUE);

    std::vector<std::string> filenames;
    filenames.push_back(inputFile);

    // Create gallery event loop
    gallery::Event ev(filenames);

    TFile* outFile = TFile::Open(outputFile.c_str(), "RECREATE");

    std::vector<TString> planeLabels = {"WW", "WE", "EW", "EE"};
    std::vector<TH1F*> hPeakAmplitude = {};
    std::vector<TH1F*> hNHits = {};
    std::vector<TH1F*> hRMS = {};
    std::vector<TH1F*> hIntegral = {};
    std::vector<TH1F*> hGoodnessOfFit = {};
    std::vector<TH1F*> hHitSummedADC = {};
    std::vector<TH1F*> hROISummedADC = {};
    std::vector<TH1F*> hChannel = {};

    TCanvas* c_wire = nullptr;

    for ( auto& id : planeLabels){
        hPeakAmplitude.push_back(new TH1F(Form("hPeakAmplitude_%s", id.Data()), Form("Hit Peak Amplitude %s;Amplitude;Counts", id.Data()), 400, 0, 400));
        hNHits.push_back(new TH1F(Form("hNHits_%s", id.Data()), Form("Number of Hits %s;Number of Hits;Counts", id.Data()), 250, 0, 25000));
        hRMS.push_back(new TH1F(Form("hRMS_%s", id.Data()), Form("Hit RMS %s;RMS;Counts", id.Data()), 100, 0, 20));
        hIntegral.push_back(new TH1F(Form("hIntegral_%s", id.Data()), Form("Hit Integral %s;Integral;Counts", id.Data()), 100, 0, 1000));
        hGoodnessOfFit.push_back(new TH1F(Form("hGoodnessOfFit_%s", id.Data()), Form("Hit Goodness of Fit %s;Goodness of Fit;Counts", id.Data()), 50, 0, 5));
        hHitSummedADC.push_back(new TH1F(Form("hHitSummedADC_%s", id.Data()), Form("Hit Summed ADC %s;Hit Summed ADC;Counts", id.Data()), 500, 0, 1000));
        hROISummedADC.push_back(new TH1F(Form("hROISummedADC_%s", id.Data()), Form("ROI Summed ADC %s;ROI Summed ADC;Counts", id.Data()), 500, 0, 1000));
        hChannel.push_back(new TH1F(Form("hChannel_%s", id.Data()), Form("Hit Channel %s;Channel;Counts", id.Data()), 3500, 0, 3500));
    }

    int evtCounter = 0;
    // Loop over events
    while (!ev.atEnd()) {
        
        auto const& hitsWW =
           *ev.getValidHandle<std::vector<recob::Hit>>({"gaushit2dTPCWW"});
        auto const& hitsWE =
           *ev.getValidHandle<std::vector<recob::Hit>>({"gaushit2dTPCWE"});
        auto const& hitsEW =
           *ev.getValidHandle<std::vector<recob::Hit>>({"gaushit2dTPCEW"});
        auto const& hitsEE =
           *ev.getValidHandle<std::vector<recob::Hit>>({"gaushit2dTPCEE"});

        auto const& wireEE =
           *ev.getValidHandle<std::vector<recob::ChannelROI>>({"wire2channelroi2d", "PHYSCRATEDATATPCEE"});
        auto const& wireEW =
           *ev.getValidHandle<std::vector<recob::ChannelROI>>({"wire2channelroi2d", "PHYSCRATEDATATPCEW"});
        auto const& wireWE =
           *ev.getValidHandle<std::vector<recob::ChannelROI>>({"wire2channelroi2d", "PHYSCRATEDATATPCWE"});
        auto const& wireWW =
           *ev.getValidHandle<std::vector<recob::ChannelROI>>({"wire2channelroi2d", "PHYSCRATEDATATPCWW"});

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

        // Note this needs to be manually changed to the correct subdetector 
        if (evtCounter == 0){
            c_wire = wireDraw(hitsEW, wireEW, channel, timeLow, timeHigh);
        }
        ev.next();
        evtCounter++;

    }

    outFile->cd();
    for (size_t i = 0; i < planeLabels.size(); ++i) {
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
    c_wire->Write("WireWaveformWithHits");
    outFile->Close();
}
