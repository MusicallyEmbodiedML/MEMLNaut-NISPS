// Integration test: boots a real (dedicated) scsynth with the built NISPS
// plugin, instantiates it in a SynthDef, and exercises every gesture and
// control the plugin exposes — checking the server stays alive and
// responsive throughout, and that every output value stays finite and in
// [0,1]. This is the "does it actually load and not crash scsynth" check;
// 02_shaping_test.scd covers whether like/dislike measurably reshape output.
//
// Run via tests/sc/run_tests.sh, which sets NISPS_BUILD_DIR and
// SC_PLUGINS_DIR and passes --include-path for classes/.
(
var buildDir = "NISPS_BUILD_DIR".getenv ?? { Error("NISPS_BUILD_DIR env var not set").throw };
var stdPluginsDir = "SC_PLUGINS_DIR".getenv ?? { Error("SC_PLUGINS_DIR env var not set").throw };
var s = Server(\nisps_test1, NetAddr("127.0.0.1", 57150));
var pass = 0, fail = 0;
var assertTrue, oscErrors = List();

assertTrue = { |cond, msg|
	if (cond) {
		pass = pass + 1;
		"[PASS] %".format(msg).postln;
	} {
		fail = fail + 1;
		"[FAIL] %".format(msg).postln;
	};
};

s.options.ugenPluginsPath = [buildDir, stdPluginsDir];
s.options.maxLogins = 1;

OSCFunc({ |msg| oscErrors.add(msg); }, '/fail').permanent_(true);

fork {
	var synth, numOutputs = 4, replies = List();
	var replyFunc;

	s.bootSync;
	assertTrue.(s.serverRunning, "server boots with NISPS plugin present");

	SynthDef(\nispsGestureTest, {
		var ins = [ MouseX.kr(0, 1), MouseY.kr(0, 1), LFNoise1.kr(1).range(0, 1) ];
		var outs = NISPS.kr(ins, numOutputs,
			like: \like.kr(0), dislike: \dislike.kr(0), randomise: \randomise.kr(0),
			jolt: \jolt.kr(0), clearMemory: \clearMemory.kr(0),
			lrScale: \lrScale.kr(1.0), rewardScale: \rewardScale.kr(1.0),
			noise: \noise.kr(0.2), trainDivisor: \trainDivisor.kr(1));
		SendReply.kr(Impulse.kr(20), '/nispsOut', outs);
	}).add;

	s.sync;
	assertTrue.(SynthDescLib.global.at(\nispsGestureTest).notNil, "SynthDef compiles with NISPS.kr inside it");

	replyFunc = OSCFunc({ |msg| replies.add(msg[3..]); }, '/nispsOut', s.addr).permanent_(true);

	// Target the root node explicitly: on this machine's SC 3.15.0-dev build,
	// a plain Synth.new (implicit target = s.defaultGroup) silently fails to
	// create ANY node at all -- reproduced even with a completely vanilla
	// Out.kr(0, DC.kr(0.5)) SynthDef with no NISPS involved, so it's a
	// pre-existing local-build/environment quirk, not something to work
	// around inside the plugin. Explicit root-node targeting sidesteps it.
	synth = Synth.head(s, \nispsGestureTest);
	s.sync;
	0.3.wait; // let a few control blocks and SendReply pings land

	assertTrue.(s.serverRunning, "server survives instantiating NISPS");
	assertTrue.(replies.size > 0, "received at least one SendReply from the running synth");

	// Exercise every gesture / control at least once.
	synth.set(\like, 1); s.sync; 0.05.wait; synth.set(\like, 0);
	synth.set(\dislike, 1); s.sync; 0.05.wait; synth.set(\dislike, 0);
	synth.set(\randomise, 1); s.sync; 0.05.wait; synth.set(\randomise, 0);
	synth.set(\clearMemory, 1); s.sync; 0.05.wait; synth.set(\clearMemory, 0);

	synth.set(\jolt, 1); s.sync;
	0.2.wait; // hold the jolt gate across several control blocks
	synth.set(\jolt, 0); s.sync;

	synth.set(\lrScale, 0.5); synth.set(\rewardScale, 2.0); synth.set(\noise, 0.4); synth.set(\trainDivisor, 4);
	s.sync;
	0.2.wait;

	assertTrue.(s.serverRunning, "server survives every gesture (like/dislike/randomise/jolt/clearMemory) and control change");
	assertTrue.(oscErrors.size == 0, "no /fail messages received from the server (got %)".format(oscErrors.size));

	replies = replies[max(0, replies.size - 50)..]; // just the tail, post-gesture-exercise
	assertTrue.(
		replies.every { |r| r.every { |v| v.isNumber and: { v.isNaN.not } and: { v >= 0.0 and: { v <= 1.0 } } } },
		"all % observed output samples are finite and in [0,1]".format(replies.size)
	);
	assertTrue.(
		replies.every { |r| r.size == numOutputs },
		"every SendReply carries exactly numOutputs (%) values".format(numOutputs)
	);

	synth.free;
	replyFunc.free;
	s.quit;
	0.3.wait;

	"\n==== 01_load_and_gestures_test: % passed, % failed ====\n".format(pass, fail).postln;
	(fail == 0).if({ "SC_TEST_RESULT: PASS".postln }, { "SC_TEST_RESULT: FAIL".postln });
	0.exit;
};
)
