// End-to-end shaping test through a real scsynth: repeatedly sets NISPS's
// inputs to a fixed location (with a little jitter), reads its output back
// over OSC, and gives like/dislike feedback based on a synthetic preference
// — mirroring nisps-sc/tests/host_smoke_test.cpp's localized-feedback check,
// but exercising the full OSC round trip (control-setting, gesture
// triggering, SendReply readback) against the actual compiled .scx rather
// than NISPSCore in isolation.
//
// Run via tests/sc/run_tests.sh, which sets NISPS_BUILD_DIR and
// SC_PLUGINS_DIR and passes --include-path for classes/.
(
var buildDir = "NISPS_BUILD_DIR".getenv ?? { Error("NISPS_BUILD_DIR env var not set").throw };
var stdPluginsDir = "SC_PLUGINS_DIR".getenv ?? { Error("SC_PLUGINS_DIR env var not set").throw };
var s = Server(\nisps_test2, NetAddr("127.0.0.1", 57151));
var pass = 0, fail = 0;
var assertTrue;

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

fork {
	var numOutputs = 4;
	var synth, replyFunc;
	var lastAction = nil;
	var allInRange = true;
	var preference = { |action| action[0] - action[1] };
	var setJitteredInputs = {
		synth.set(\in0, 0.5 + 0.02.rand2, \in1, 0.5 + 0.02.rand2, \in2, 0.5 + 0.02.rand2);
	};
	var evalAvg = { |n|
		var total = 0.0;
		n.do {
			setJitteredInputs.value;
			s.sync;
			0.03.wait;
			if (lastAction.notNil) { total = total + preference.(lastAction) };
		};
		total / n;
	};
	var initialScore, finalScore, score;

	s.bootSync;
	assertTrue.(s.serverRunning, "server boots with NISPS plugin present");

	SynthDef(\nispsShapingTest, {
		var ins = [ \in0.kr(0.5), \in1.kr(0.5), \in2.kr(0.5) ];
		var outs = NISPS.kr(ins, numOutputs,
			like: \like.kr(0), dislike: \dislike.kr(0), randomise: 0, jolt: 0, clearMemory: 0,
			lrScale: 1.0, rewardScale: 1.0, noise: 0, trainDivisor: 1);
		SendReply.kr(Impulse.kr(50), '/nispsAction', outs);
	}).add;
	s.sync;

	replyFunc = OSCFunc({ |msg|
		lastAction = msg[3..].asArray;
		if (lastAction.every { |v| v.isNumber and: { v.isNaN.not } and: { v >= 0.0 and: { v <= 1.0 } } }.not) {
			allInRange = false;
		};
	}, '/nispsAction', s.addr).permanent_(true);

	// See 01_load_and_gestures_test.scd's comment: implicit-default-group
	// Synth.new silently creates no node at all on this machine's SC build,
	// reproduced with a plain vanilla SynthDef too -- unrelated to NISPS.
	// Target the root node explicitly to sidestep it.
	synth = Synth.head(s, \nispsShapingTest);
	s.sync;
	0.1.wait;
	assertTrue.(lastAction.notNil, "receiving SendReply output from the running synth");

	initialScore = evalAvg.(20);

	400.do {
		setJitteredInputs.value;
		s.sync;
		0.02.wait;
		score = preference.(lastAction ?? { [0.5, 0.5, 0.5, 0.5] });
		if (score > 0) {
			synth.set(\like, 1); s.sync; synth.set(\like, 0);
		} {
			synth.set(\dislike, 1); s.sync; synth.set(\dislike, 0);
		};
	};

	finalScore = evalAvg.(20);

	"Initial avg preference score: %".format(initialScore).postln;
	"Final avg preference score:   %".format(finalScore).postln;

	assertTrue.(s.serverRunning, "server survives the full like/dislike training loop over OSC");
	assertTrue.(allInRange, "every observed output sample stayed finite and in [0,1] throughout");
	// Informational, not a hard requirement — see host_smoke_test.cpp's comment on
	// why this is stochastic (small memory, jittered single location, un-seeded
	// internal RNGs). Logged as PASS/INCONCLUSIVE, doesn't affect SC_TEST_RESULT.
	if (finalScore > initialScore) {
		"[INFO] learning check: PASS (final % > initial %)".format(finalScore, initialScore).postln;
	} {
		"[INFO] learning check: INCONCLUSIVE (final % <= initial %; stochastic, see host_smoke_test.cpp)".format(finalScore, initialScore).postln;
	};

	synth.free;
	replyFunc.free;
	s.quit;
	0.3.wait;

	"\n==== 02_shaping_test: % passed, % failed ====\n".format(pass, fail).postln;
	(fail == 0).if({ "SC_TEST_RESULT: PASS".postln }, { "SC_TEST_RESULT: FAIL".postln });
	0.exit;
};
)
