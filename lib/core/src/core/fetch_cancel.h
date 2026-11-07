#pragma once

// Stopping a running fetch early, so a button press is not kept waiting.
//
// The main loop runs the fetches of the data sources one after the other;
// a download takes up to several seconds (the calendar), a failing one up
// to the timeout of core/http.h. A press latched by the interrupt is only
// evaluated after it. With a cancel check set (src/main.cpp: a press is
// waiting and the caller is the task of the main loop), long work stops
// early: the HTTPS body reader of core/http.cpp asks fetchShouldStop() while
// it reads, and a request does not start at all while a press waits. The
// display controller then counts the fetch neither as a success nor as a
// failure and repeats it right after the press has been handled.
//
// Not stoppable: the TLS handshake inside the network library (normally
// well under a second, at most the timeout of core/http.h).
//
// The check runs in the task that does the work; it must be quick (it is
// asked every few hundred bytes).

#include <functional>

namespace core {

// Sets the check; an empty function removes it (nothing is ever stopped).
void setFetchCancelCheck(std::function<bool()> check);

// Whether a stop is wanted now, without remembering it (e.g. before the
// next fetch of a pass).
bool fetchStopPending();

// For long work inside a fetch: whether to stop now. A true answer is
// remembered until takeFetchCancelled().
bool fetchShouldStop();

// Whether the work since the last call was stopped by fetchShouldStop();
// resets it.
bool takeFetchCancelled();

}  // namespace core
