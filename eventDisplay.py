import os, sys
import ROOT as r
import argparse

def parse_args():
	parser = argparse.ArgumentParser(description="Hit Tuning Parameter Scan")
	parser.add_argument('-o', '--outputDir', type=str, default='eventDisplays', help='Output directory')
	parser.add_argument('--tag', type=str, default='test', help='Tag for output files')
	parser.add_argument('-d', '--debug', action='store_true', help='Enable debug mode')
	parser.add_argument('-v', '--verbose', action='store_true', help='Enable verbose output')
	parser.add_argument('-i', '--inputFile', type=str, default='', help='Input file to process')
	parser.add_argument('-e', '--eventNumber', type=int, default=0, help='Event number to display')
	parser.add_argument('-p', '--plane', type=int, default=0, help='Plane number to display (0, 1, or 2)')
	parser.add_argument('-t', '--timeRange', type=int, nargs=2, default=[0, 5000], help='Time range to display (min max)')
	parser.add_argument('-w', '--wireRange', type=int, nargs=2, default=[0, 0], help='Wire range to display (min max)')
	return parser.parse_args()

def read_header(h):
	r.gROOT.ProcessLine(f'#include "{h}"')

def initialize():
	print("Reading headers...")
	read_header("gallery/Event.h")
	read_header("gallery/ValidHandle.h")
	read_header("nusimdata/SimulationBase/MCTruth.h")
	read_header("canvas/Utilities/InputTag.h")
	read_header("lardataobj/RecoBase/Hit.h")
	read_header("lardataobj/RecoBase/Wire.h")
	read_header("gallery/Handle.h")

	print("Loading libraries...")
	r.gSystem.Load("libgallery")
	r.gSystem.Load("libnusimdata_SimulationBase")
	r.gSystem.Load("libart_Framework_Principal")

	r.gROOT.SetBatch(1)
	r.gStyle.SetOptStat(0)

def getBounds(plane, det):
	if plane == 0:
		if det == 'EE': return [0, 2239]
		elif det == 'EW': return [13824, 16063]
		elif det == 'WE': return [27648, 29887]
		elif det == 'WW': return [41472, 43711]
	elif plane == 1:
		if det == 'EE': return [2304, 8063]
		elif det == 'EW': return [16128, 21087]
		elif det == 'WE': return [29952, 35711]
		elif det == 'WW': return [43776, 49535]
	elif plane == 2:
		if det == 'EE': return [8064, 13823]
		elif det == 'EW': return [21888, 27647]
		elif det == 'WE': return [35712, 41471]
		elif det == 'WW': return [49536, 55295]
	return [0, 0]

if __name__ == "__main__":
	
	args = parse_args()

	tag = args.tag
	plane = args.plane
	filenames = r.vector(r.string)(1, args.inputFile)
	eventNumber = int(args.eventNumber)
	r_time = args.timeRange
	wire_bounds = args.wireRange

	initialize()

	histfile = r.TFile(f"{args.outputDir}/display_{tag}_evt{eventNumber}_plane{plane}.root", "RECREATE")
	npart_hist = r.TH1F("npart", "Number of Hits", 51, -0.5, 50.5)

	print("Preparing before event loop...")
	hitsEE_tag = r.art.InputTag("gaushit2dTPCEE", "")
	wireEE_tag = r.art.InputTag("channel2wire", "PHYSCRATEDATATPCEE")
	hitsEW_tag = r.art.InputTag("gaushit2dTPCEW", "")
	wireEW_tag = r.art.InputTag("channel2wire", "PHYSCRATEDATATPCEW")
	hitsWE_tag = r.art.InputTag("gaushit2dTPCWE", "")
	wireWE_tag = r.art.InputTag("channel2wire", "PHYSCRATEDATATPCWE")
	hitsWW_tag = r.art.InputTag("gaushit2dTPCWW", "")
	wireWW_tag = r.art.InputTag("channel2wire", "PHYSCRATEDATATPCWW")

	print("Creating event object ...")
	ev = r.gallery.Event(filenames)

	GetVH_Hits = r.gallery.Event.getValidHandle['std::vector<recob::Hit>']
	GetVH_Wire = r.gallery.Event.getValidHandle['std::vector<recob::Wire>']

	c1 = r.TCanvas("c1", "c1", 1200, 800)

	print("Entering event loop...")
	event = 0
	while not ev.atEnd():

		event = ev.eventAuxiliary().event()
		run = ev.eventAuxiliary().run()
		print(f"Run {run}, event {event}")

		if event != eventNumber: 
			ev.next()
			continue

		print("found event to display!", eventNumber)
		
		# Call the specialized member; handle missing product gracefully
		hitsEE_handle = GetVH_Hits(ev, hitsEE_tag)
		wireEE_handle = GetVH_Wire(ev, wireEE_tag)
		hitsEW_handle = GetVH_Hits(ev, hitsEW_tag)
		wireEW_handle = GetVH_Wire(ev, wireEW_tag)
		hitsWE_handle = GetVH_Hits(ev, hitsWE_tag)
		wireWE_handle = GetVH_Wire(ev, wireWE_tag)
		hitsWW_handle = GetVH_Hits(ev, hitsWW_tag)
		wireWW_handle = GetVH_Wire(ev, wireWW_tag)

		hit_handles = [hitsEE_handle, hitsEW_handle, hitsWE_handle, hitsWW_handle]
		wire_handles = [wireEE_handle, wireEW_handle, wireWE_handle, wireWW_handle]
		quadrant_tags = ["EE", "EW", "WE", "WW"]

		for idet, det in enumerate(["EE", "EW", "WE", "WW"]):
			
			wire_handle = wire_handles[idet]
			hits_handle = hit_handles[idet]

			bounds = getBounds(plane, det)
			print(f"Bounds for plane {plane} det {det}: {bounds}")
			if wire_bounds[0] == 0 and wire_bounds[1] == 0:
				r_wires = [bounds[0], bounds[1]]
			else:
				r_wires = [wire_bounds[0], wire_bounds[1]]

			print(f"Displaying {det} plane {plane} with wire range {r_wires} and time range {r_time}")
			h_wire2d = r.TH2F(f"h_wire2d{event}_{det}", f"Display: Run {run} Event {event} {det};Channel;Time Ticks", r_wires[1]-r_wires[0], r_wires[0], r_wires[1], r_time[1]-r_time[0], r_time[0], r_time[1])

			if wire_handle:
				wire = wire_handle.product()
				nWires = wire.size()
				nTicks = wire[1].NSignal()
				print(f"There are {nWires} wires with {nTicks} clock ticks")
				

				for iw, w in enumerate(wire):
					if (args.verbose): print(f"processing {det} wire {w.Channel()}")
					if w.Channel() < r_wires[0]: continue
					if w.Channel() > r_wires[1]: continue

					totalTicks = w.NSignal()
					for it, t in enumerate(w.Signal()):
						if it < r_time[0] or it > r_time[1]: continue
						binX_ = h_wire2d.GetXaxis().FindBin(w.Channel())
						binY_ = h_wire2d.GetYaxis().FindBin(it)
						binY_ = h_wire2d.GetNbinsY() - binY_

						h_wire2d.SetBinContent(binX_, binY_, w.Signal()[it])

				
				c1.cd()
				if h_wire2d.GetMinimum() >= 0: h_wire2d.GetZaxis().SetRangeUser(-1, h_wire2d.GetMaximum()*1.2)
				#c1.SetLogz()
				#h_wire2d.SetMinimum(0)
				h_wire2d.Draw("colz")
				c1.SaveAs(f"{args.outputDir}/event_{tag}_run{run}_evt{event}_plane{plane}_{det}.png")
				histfile.cd()
				h_wire2d.Write(f"wire_{event}")

			if hits_handle:
				hits = hits_handle.product() 
				
				# Count hits per event (or iterate to fill a hit attribute)
				if (args.verbose): print("Number of hits", hits.size())

				c_hits2d = r.TCanvas(f"c_hits2d_{det}", "Hits", 800, 800)

				c1.cd()
				for ih, h in enumerate(hits):
					chan = h.Channel()
					if chan < r_wires[0] or chan > r_wires[1]: continue

					mean = h.PeakTime()
					if mean < r_time[0] or mean > r_time[1]: continue

					rad = float(h.RMS())

					#convert mean time to flip y axis
					mean = r_time[1] - mean + r_time[0]

					ellipse = r.TEllipse(chan, mean, 1, rad)
					ellipse.SetFillStyle(1001)
					ellipse.SetFillColorAlpha(r.kRed, 0.2)
					ellipse.SetLineColor(r.kRed)
					ellipse.SetLineWidth(0)

					ellipse.Draw("f same")
					h_wire2d.GetListOfFunctions().Add(ellipse)

				histfile.cd()
				h_wire2d.Write(f"c_hits2d_{det}_{event}")


				npart_hist.Fill(hits.size())

			print("Writing histograms...")
			histfile.cd()
			c1.Write(f"c1_{det}")
			c1.SaveAs(f"{args.outputDir}/eventHits_{tag}_run{run}_evt{event}_plane{plane}_{det}.png")

		event += 1
		break


	histfile.Write()
	histfile.Close()
