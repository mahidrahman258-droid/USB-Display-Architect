import { useState } from 'react';
import { Layout, Server, Sliders, Cpu, Activity, Lightbulb, Terminal, Cable, FileCode2, HelpCircle } from 'lucide-react';

// Import our custom interactive modules
import PipelineSimulator from './components/PipelineSimulator';
import ProtocolBuilder from './components/ProtocolBuilder';
import ArchitectureVisualizer from './components/ArchitectureVisualizer';
import SpecsDoc from './components/SpecsDoc';
import CodeSnippets from './components/CodeSnippets';

export default function App() {
  const [activeWorkspace, setActiveWorkspace] = useState<string>('simulator');

  const workspaceModes = [
    { id: 'simulator', label: 'Frame Stream Simulator', icon: Sliders, desc: 'Calculate frame latencies & bitrates' },
    { id: 'architecture', label: 'Interactive Architecture', icon: Cpu, desc: 'Explore kernel driver hooks & pipelines' },
    { id: 'specs', label: 'Systems Deep-Dive', icon: Server, desc: 'Technical specs & competitor secrets' },
    { id: 'code', label: 'Production Boilerplates', icon: FileCode2, desc: 'C++ & Kotlin direct memory loops' },
  ];

  return (
    <div className="min-h-screen bg-slate-950 font-sans text-slate-200 flex flex-col">
      {/* Top Navigation Header */}
      <header className="bg-slate-900 border-b border-slate-800 px-6 py-4 sticky top-0 z-40 shadow-[0_4px_12px_rgba(0,0,0,0.5)]">
        <div className="max-w-7xl mx-auto flex flex-col sm:flex-row justify-between items-start sm:items-center gap-4">
          
          <div>
            <div className="flex items-center gap-2.5">
              <span className="p-2.5 bg-cyan-950/80 text-cyan-400 rounded-lg border border-cyan-800/80 shadow-[0_0_12px_rgba(6,182,212,0.15)] flex items-center justify-center">
                <Cable className="w-5 h-5 animate-pulse" />
              </span>
              <div>
                <h1 className="text-xl font-bold text-white font-sans tracking-tight flex items-center gap-2">
                  USB DISPLAY <span className="text-cyan-400 font-mono tracking-widest text-sm bg-cyan-950/60 px-1.5 py-0.5 rounded border border-cyan-800/50">ARCHITECT</span>
                </h1>
                <p className="text-[10px] text-cyan-500 font-mono uppercase tracking-wider mt-1.5">
                  SYSTEM ENGINEERING & ULTRA-LOW LATENCY PIPELINE SIMULATORS
                </p>
              </div>
            </div>
          </div>

          <div className="flex items-center gap-3">
            <div className="px-3 py-1.5 bg-slate-950 rounded-lg border border-slate-800 flex items-center gap-2 text-xs font-mono">
              <Activity className="w-3.5 h-3.5 text-cyan-400 animate-pulse" />
              <span className="text-slate-500">Pipeline State:</span>
              <span className="text-cyan-400 font-bold">READY</span>
            </div>
            
            <a
              href="#specs-link"
              onClick={(e) => {
                e.preventDefault();
                setActiveWorkspace('specs');
              }}
              className="px-4 py-1.5 text-xs font-mono font-bold uppercase rounded-lg bg-cyan-950 hover:bg-cyan-900 text-cyan-400 border border-cyan-500 shadow-[0_0_10px_rgba(6,182,212,0.1)] hover:shadow-[0_0_15px_rgba(6,182,212,0.25)] transition-all"
            >
              System Specs
            </a>
          </div>

        </div>
      </header>

      {/* Primary Workspace Body */}
      <main className="flex-grow max-w-7xl w-full mx-auto p-4 sm:p-6 lg:p-8 grid grid-cols-1 lg:grid-cols-12 gap-8">
        
        {/* Navigation Sidebar (3 Columns) */}
        <div className="lg:col-span-3 space-y-4">
          <div className="bg-slate-900 rounded-xl border border-slate-800 p-4 shadow-lg">
            <span className="text-[10px] font-mono font-bold text-cyan-400 block uppercase tracking-widest px-1 mb-3">
              // CONTROL_CENTER
            </span>
            
            <div className="space-y-1.5">
              {workspaceModes.map((mode) => {
                const Icon = mode.icon;
                const isActive = activeWorkspace === mode.id;
                return (
                  <button
                    key={mode.id}
                    onClick={() => setActiveWorkspace(mode.id)}
                    className={`w-full flex items-start gap-3 p-3 rounded-lg transition-all text-left ${
                      isActive
                        ? 'bg-cyan-950/40 text-cyan-300 border border-cyan-800 shadow-[0_0_10px_rgba(6,182,212,0.05)]'
                        : 'border border-transparent hover:bg-slate-850 text-slate-400 hover:text-slate-200'
                    }`}
                  >
                    <span className={`p-1.5 rounded mt-0.5 ${
                      isActive 
                        ? 'bg-cyan-500 text-slate-950 shadow-[0_0_8px_rgba(6,182,212,0.4)]' 
                        : 'bg-slate-800 text-slate-400'
                    }`}>
                      <Icon className="w-3.5 h-3.5" />
                    </span>
                    <div>
                      <span className="font-bold text-xs block leading-tight font-sans">
                        {mode.label}
                      </span>
                      <span className="text-[10px] text-slate-500 block leading-normal mt-0.5 font-mono">
                        {mode.desc}
                      </span>
                    </div>
                  </button>
                );
              })}
            </div>
          </div>

          {/* Quick Architecture Reference Callout */}
          <div className="bg-slate-900 text-slate-100 rounded-xl p-5 border border-slate-800 shadow-xl relative overflow-hidden">
            <div className="absolute top-0 right-0 w-24 h-24 bg-cyan-500/5 rounded-full blur-2xl pointer-events-none"></div>
            <div className="flex items-center gap-1.5 text-xs font-mono font-bold text-cyan-400 uppercase tracking-widest mb-3.5">
              <Lightbulb className="w-4 h-4 text-cyan-400" />
              <span>[Architect.log]</span>
            </div>
            <p className="text-[11px] leading-relaxed text-slate-400 font-sans">
              Designing second screens via USB requires establishing an <strong className="text-slate-200 font-semibold font-mono text-[10px] bg-slate-950 px-1 py-0.5 rounded border border-slate-800">Indirect Display Driver (WDDM 2.0)</strong> on Windows and tunneling compressed, low-latency H.264 video streams over <strong className="text-slate-200 font-semibold font-mono text-[10px] bg-slate-950 px-1 py-0.5 rounded border border-slate-800">USB Bulk Endpoint pipes</strong> for target display decoding via Kotlin's low-overhead <strong className="text-slate-200 font-semibold font-mono text-[10px] bg-slate-950 px-1 py-0.5 rounded border border-slate-800">MediaCodec API</strong>.
            </p>
            
            <div className="mt-4 pt-3 border-t border-slate-800 flex justify-between items-center text-[10px] text-slate-500 font-mono">
              <span>Bus: USB 3.0 Standard</span>
              <span className="text-cyan-400 font-bold">Latency: ~21ms</span>
            </div>
          </div>
        </div>

        {/* Dynamic Display Panel (9 Columns) */}
        <div id="dynamic-display-panel" className="lg:col-span-9 space-y-8">
          
          {/* Mode 1: Simulator */}
          {activeWorkspace === 'simulator' && (
            <>
              <PipelineSimulator />
              <ProtocolBuilder />
            </>
          )}

          {/* Mode 2: Interactive Architecture */}
          {activeWorkspace === 'architecture' && (
            <ArchitectureVisualizer />
          )}

          {/* Mode 3: Specifications Manual */}
          {activeWorkspace === 'specs' && (
            <SpecsDoc />
          )}

          {/* Mode 4: Source Code Boilerplates */}
          {activeWorkspace === 'code' && (
            <CodeSnippets />
          )}

        </div>

      </main>

      {/* Footer */}
      <footer className="bg-slate-900 border-t border-slate-800 py-6 text-center px-4 mt-12 text-xs text-slate-500">
        <div className="max-w-7xl mx-auto flex flex-col sm:flex-row justify-between items-center gap-3 font-mono">
          <p>
            USB Display Architect Console &copy; 2026. Custom systems-level second-screen development platform.
          </p>
          <div className="flex items-center gap-4 font-bold uppercase tracking-wider text-[10px]">
            <a href="#simulator" onClick={() => setActiveWorkspace('simulator')} className="hover:text-cyan-400 transition-colors">Simulator</a>
            <a href="#architecture" onClick={() => setActiveWorkspace('architecture')} className="hover:text-cyan-400 transition-colors">Bento Map</a>
            <a href="#specs" onClick={() => setActiveWorkspace('specs')} className="hover:text-cyan-400 transition-colors">Manual API</a>
          </div>
        </div>
      </footer>
    </div>
  );
}
