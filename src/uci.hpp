#pragma once

// Runs the UCI protocol loop on stdin/stdout until `quit` or EOF. Search
// runs on a background thread with an atomic stop flag, so `stop` is
// genuinely preemptive (unlike the JS engine's synchronous, single-threaded
// version, where `stop` could only be honored between top-level `go` calls).
void runUciLoop();
