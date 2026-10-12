NISPS : MultiOutUGen {
	// inputArray: the N control-rate signals to map (joystick axes, machine-
	// listening features, MIDI CCs, LFOs — anything at all).
	// numOutputs: how many mapped-parameter outputs to produce. Must be a
	// plain Integer, not a UGen — like In.kr's numChannels, it sizes the
	// output array when the SynthDef graph is built, before the underlying
	// Unit exists, so the server has to know it up front.
	//
	// Gestures (like/dislike/randomise/clearMemory) are one-shot triggers:
	// any transition from <= 0 to > 0 fires the gesture once. jolt is a gate:
	// held > 0 for as long as the weight-morph should run, like the
	// hardware's momentary button.
	*kr { |inputArray, numOutputs = 4, like = 0, dislike = 0, randomise = 0, jolt = 0,
		clearMemory = 0, lrScale = 1.0, rewardScale = 1.0, noise = 0.2, trainDivisor = 1|
		^this.multiNewList(['control', numOutputs, like, dislike, randomise, jolt,
			clearMemory, lrScale, rewardScale, noise, trainDivisor] ++ inputArray.asArray)
	}

	init { |numOutputs ... theRest|
		// numOutputs is consumed here and never becomes an actual UGen input
		// (mirrors In/LocalIn in the class library) — the remaining args are
		// the real per-sample inputs the C++ unit reads.
		inputs = theRest.asArray;
		^this.initOutputs(numOutputs, rate)
	}
}
