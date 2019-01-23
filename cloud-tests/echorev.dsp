import("stdfaust.lib");

// User interface with MIDI convention
duration = hslider("duration[unit:ms]", 250, 0, 1000, 1)/1000;
feedback = hslider("feedback", 0.7, 0, 1, 0.01) : en.adsr(0.01,0.01,0.9,0.1);

// echo + reverb
process = par(i,2, ef.echo(2, duration, feedback)) : dm.zita_light;
