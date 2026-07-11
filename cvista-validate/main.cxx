// SPDX-License-Identifier: BSD-3-Clause
//
// cvista SMP parallel-vs-serial parity validator.
//
// cvista defaults the library-wide vtkSMPTools backend to STDThread (see
// cvista-config/minimal.cmake), so every vtkSMPTools::For loop runs
// multithreaded out of the box. A Kitware reviewer raised the concern that
// parallelizing an algorithm can make its output unstable -- i.e. differ from
// the single-threaded result, or vary run-to-run with thread scheduling.
//
// This program settles that empirically. For each algorithm it computes a
// SERIAL reference and then re-runs it under the STDThread backend at several
// thread counts, repeated, and asserts the output is BYTE-EXACT every time:
//
//   serial  reference : CVISTA_SMP_DEFAULT=0 + SetBackend("Sequential") + Initialize(1)
//                       (a true serial floor -- also disables the 4-thread
//                        opt-in that the RunSafeFilterParallel filters would
//                        otherwise take even under the Sequential backend)
//   parallel run(s)   : SetBackend("STDThread") + Initialize(T),
//                       T in {2,4,8,oversubscribed}, each repeated N times
//                       (repeats catch scheduling-dependent nondeterminism)
//
// Most filters must be BYTE-EXACT parallel-vs-serial. A few (parallel cut/
// contour extraction) emit the same geometry in a thread-dependent ORDER; those
// are tagged orderRelaxed and instead must be (a) run-to-run deterministic
// (repeated parallel runs byte-identical -- the real instability check) and
// (b) equal to serial as an order-insensitive geometry set. Exit code is nonzero
// if any algorithm fails its policy -- a CI gate on the "threaded == serial" claim.
//
// DIAGNOSTIC hooks (see ci/run-smp-parity.sh): a SIGSEGV/SIGABRT handler prints the
// CASE + PHASE that was executing plus a backtrace, so any crash -- e.g. the
// intermittent Linux heap corruption -- names the offending filter. With
// CVISTA_FORK_ISOLATE=1 each case runs in its own forked child, so a crash is
// attributed to exactly one case (and cannot abort the whole sweep). Both are inert
// on the normal gate (in-process, no fork); the handler only fires on a real crash.

#include "smpParityCases.h"
#include "smpParityCompare.h"

#include <vtkAlgorithm.h>
#include <vtkDataObject.h>
#include <vtkNew.h>
#include <vtkSMPTools.h>
#include <vtkSmartPointer.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <csignal>
#include <execinfo.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifdef _WIN32
static void setEnvVar(const char* k, const char* v)
{
  _putenv_s(k, v);
}
static void unsetEnvVar(const char* k)
{
  _putenv_s(k, ""); // empty value removes the variable on Windows
}
#else
static void setEnvVar(const char* k, const char* v)
{
  setenv(k, v, 1);
}
static void unsetEnvVar(const char* k)
{
  unsetenv(k);
}
#endif

namespace
{

// ---- DIAGNOSTIC: current-case tracking + crash handler ----------------------
// Updated (single-threaded) right before each phase runs, so if a threaded filter
// corrupts the heap and the process later faults, the handler names what was
// running. volatile + a static phase buffer keep the handler's read simple.
const char* volatile g_case = "<none>";  // volatile POINTER: handler reads latest value
const char* volatile g_phase = "<init>";
char g_phaseBuf[96] = "<init>";

void setCase(const char* name)
{
  g_case = name;
}
void setPhase(const char* p)
{
  g_phase = p;
}
void setPhaseRun(int nthreads, int rep)
{
  std::snprintf(g_phaseBuf, sizeof(g_phaseBuf), "parallel T=%d rep=%d", nthreads, rep);
  g_phase = g_phaseBuf;
}

#ifndef _WIN32
extern "C" void crashHandler(int sig)
{
  const char* nm = g_case;
  const char* ph = g_phase;
  char buf[256];
  int n = std::snprintf(buf, sizeof(buf),
    "\n*** CVISTA-PARITY CRASH: signal %d while running case '%s' phase '%s' ***\n", sig,
    nm ? nm : "?", ph ? ph : "?");
  if (n > 0)
    (void)!write(STDERR_FILENO, buf, static_cast<size_t>(n));
  void* frames[96];
  int nf = backtrace(frames, 96);
  backtrace_symbols_fd(frames, nf, STDERR_FILENO); // async-signal-safe
  // Restore the default disposition and re-raise so the process actually dies with
  // the signal (WIFSIGNALED for the fork parent, + a core dump if enabled).
  signal(sig, SIG_DFL);
  raise(sig);
}

void installCrashHandler()
{
  struct sigaction sa;
  std::memset(&sa, 0, sizeof(sa));
  sa.sa_handler = crashHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_NODEFER;
  for (int s : { SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL })
    sigaction(s, &sa, nullptr);
}
#else
void installCrashHandler() {}
#endif

void forceSerial()
{
  setEnvVar("CVISTA_SMP_DEFAULT", "0"); // disable cvista's opt-in threading floor
  vtkSMPTools::SetBackend("Sequential");
  vtkSMPTools::Initialize(1);
}

void forceParallel(int threads)
{
  unsetEnvVar("CVISTA_SMP_DEFAULT");
  vtkSMPTools::SetBackend("STDThread");
  vtkSMPTools::Initialize(threads);
}

// Build the filter fresh, run it under whatever backend is currently active, and
// return an independent deep copy of its output (so it survives later runs).
vtkSmartPointer<vtkDataObject> runOnce(const smpparity::Case& c, const smpparity::Inputs& in)
{
  vtkSmartPointer<vtkAlgorithm> algo = c.make(in);
  if (!algo)
    return nullptr;
  algo->Update();
  vtkDataObject* out = algo->GetOutputDataObject(c.outputPort);
  if (!out)
    return nullptr;
  vtkSmartPointer<vtkDataObject> clone;
  clone.TakeReference(out->NewInstance());
  clone->DeepCopy(out);
  return clone;
}

enum class CaseStatus
{
  Pass,
  Fail,
  Known,
  SerialUnstable
};

struct CaseOutcome
{
  CaseStatus status = CaseStatus::Pass;
  std::string verdict;
  int comparisons = 0;
};

// Run one case's full serial-reference + parallel-repeats policy check. Sets
// g_case/g_phase as it goes so a crash names the exact case+phase. Returns the
// outcome; the caller prints the table row (so this is fork/in-process agnostic).
CaseOutcome processCase(const smpparity::Case& c, const smpparity::Inputs& inputs, int repeats,
  const std::vector<int>& threadCounts)
{
  CaseOutcome oc;
  setCase(c.name.c_str());

  setPhase("serial-ref");
  forceSerial();
  vtkSmartPointer<vtkDataObject> ref = runOnce(c, inputs);
  // A second serial run. A single-threaded filter MUST be deterministic; if these
  // two disagree the filter is nondeterministic regardless of threading (an unseeded
  // RNG, or an uninitialized/wild read), so a parallel-vs-serial comparison against a
  // moving reference is meaningless -- detect and classify that separately.
  setPhase("serial-ref2");
  forceSerial();
  vtkSmartPointer<vtkDataObject> ref2 = runOnce(c, inputs);

  if (!ref)
  {
    oc.status = CaseStatus::Fail;
    oc.verdict = "ERROR: serial run produced no output";
    return oc;
  }

  if (ref2)
  {
    const std::string sdiff = smpparity::CompareDataObjects(ref, ref2);
    if (!sdiff.empty())
    {
      oc.status = CaseStatus::SerialUnstable;
      oc.verdict =
        "SERIAL-UNSTABLE (nondeterministic single-threaded; parallel-vs-serial N/A): " + sdiff;
      if (c.knownIssue)
        oc.verdict += "  [" + c.knownIssueReason + "]";
      return oc;
    }
  }

  // "" if all parallel runs pass, else the fail verdict.
  auto runParallel = [&]() -> std::string {
    for (std::size_t t = 0; t < threadCounts.size(); ++t)
    {
      const int nthreads = threadCounts[t];
      forceParallel(nthreads);
      vtkSmartPointer<vtkDataObject> first; // first parallel output at this count
      for (int r = 0; r < repeats; ++r)
      {
        setPhaseRun(nthreads, r);
        vtkSmartPointer<vtkDataObject> out = runOnce(c, inputs);
        ++oc.comparisons;
        if (!out)
          return "FAIL: no output @T=" + std::to_string(nthreads);

        if (r == 0)
        {
          first = out;
          // vs serial: byte-exact, or (order-relaxed) same geometry set.
          std::string diff = c.orderRelaxed ? smpparity::CompareGeometrySet(ref, out)
                                            : smpparity::CompareDataObjects(ref, out);
          if (!diff.empty())
            return std::string("FAIL ") + (c.orderRelaxed ? "geometry" : "byte-exact") +
              " vs serial @T=" + std::to_string(nthreads) + ": " + diff;
        }
        else
        {
          // Run-to-run stability at a fixed thread count.
          std::string diff = c.orderRelaxed ? smpparity::CompareGeometrySet(first, out)
                                            : smpparity::CompareDataObjects(first, out);
          if (!diff.empty())
            return std::string("FAIL ") +
              (c.orderRelaxed ? "geometry unstable run-to-run" : "nondeterministic") +
              " @T=" + std::to_string(nthreads) + " rep " + std::to_string(r) + ": " + diff;
        }
      }
    }
    return "";
  };

  const std::string fail = runParallel();
  if (fail.empty())
  {
    oc.status = CaseStatus::Pass;
    oc.verdict = c.orderRelaxed
      ? "PASS (order-relaxed: geometry stable & == serial, order nondeterministic)"
      : "PASS";
    if (c.knownIssue)
      oc.verdict += " [known-issue, stable this run]";
  }
  else if (c.knownIssue)
  {
    // Documented nondeterminism (unseeded RNG, or a tracked data race). Report it
    // with full context but do NOT fail the gate.
    oc.status = CaseStatus::Known;
    oc.verdict =
      "KNOWN-ISSUE (documented, not gated) [" + c.knownIssueReason + "] observed: " + fail;
  }
  else
  {
    oc.status = CaseStatus::Fail;
    oc.verdict = fail;
  }
  return oc;
}

void printRow(const smpparity::Case& c, const std::string& verdict)
{
  std::cout << std::left << std::setw(34) << c.name << std::setw(18) << c.module << std::setw(13)
            << smpparity::RiskName(c.risk) << verdict << "\n";
}

} // namespace

int main(int argc, char* argv[])
{
  const int repeats = (argc > 1) ? std::max(1, std::atoi(argv[1])) : 4;

  installCrashHandler();

  bool forkIsolate = false;
#ifndef _WIN32
  if (const char* fe = std::getenv("CVISTA_FORK_ISOLATE"))
    forkIsolate = (std::string(fe) == "1");
#endif

  const char* def = vtkSMPTools::GetBackend();
  const unsigned hw = std::thread::hardware_concurrency();

  std::cout << "== cvista SMP parallel-vs-serial parity validator ==\n";
  std::cout << "operative default SMP backend : " << (def ? def : "(null)") << "\n";
  std::cout << "hardware_concurrency          : " << hw << "\n";
  std::cout << "repeats per (case,threadcount): " << repeats << "\n";
  std::cout << "per-case fork isolation       : " << (forkIsolate ? "ON" : "off") << "\n";

  // Thread counts to exercise: small, the cvista default cap (4), a larger power of
  // two when the box has the cores, and an oversubscribed count to stress the
  // scheduler into different work partitions.
  std::vector<int> threadCounts = { 2, 4 };
  if (hw >= 8)
    threadCounts.push_back(8);
  threadCounts.push_back(std::max(2u, hw * 2u));
  std::sort(threadCounts.begin(), threadCounts.end());
  threadCounts.erase(std::unique(threadCounts.begin(), threadCounts.end()), threadCounts.end());

  forceParallel(threadCounts.back());
  const int estThreads = vtkSMPTools::GetEstimatedNumberOfThreads();
  std::cout << "STDThread estimated threads   : " << estThreads
            << (estThreads <= 1 ? "  [WARNING: no real parallelism on this host]" : "") << "\n";
  std::cout << "thread counts exercised       : ";
  for (int t : threadCounts)
    std::cout << t << " ";
  std::cout << "\n\n";

  // Inputs are built once, under the serial floor, so they are deterministic. In
  // fork-isolate mode they are built in the PARENT before any fork, and each child
  // inherits them copy-on-write (read-only use), so no per-case rebuild cost.
  forceSerial();
  const smpparity::Inputs inputs = smpparity::BuildInputs();

  const std::vector<smpparity::Case> cases = smpparity::RegisterCases();

  int failed = 0;
  int known = 0;
  int serialUnstable = 0;
  int comparisons = 0;
  int crashed = 0;
  std::vector<std::string> crashedCases;

  std::cout << std::left << std::setw(34) << "algorithm" << std::setw(18) << "module"
            << std::setw(13) << "risk" << "result\n";
  std::cout << std::string(84, '-') << "\n";

  for (const smpparity::Case& c : cases)
  {
#ifndef _WIN32
    if (forkIsolate)
    {
      std::fflush(stdout);
      std::fflush(stderr);
      pid_t pid = fork();
      if (pid == 0)
      {
        // Child: run this one case in isolation. A crash here is attributable to
        // THIS case; the handler prints the case+phase+backtrace before dying.
        CaseOutcome oc = processCase(c, inputs, repeats, threadCounts);
        printRow(c, oc.verdict);
        std::fflush(stdout);
        int code = (oc.status == CaseStatus::Fail) ? 2
          : (oc.status == CaseStatus::Known)       ? 3
          : (oc.status == CaseStatus::SerialUnstable) ? 4
                                                      : 0;
        _exit(code);
      }
      else if (pid > 0)
      {
        int st = 0;
        while (waitpid(pid, &st, 0) < 0 && errno == EINTR)
        {
        }
        if (WIFSIGNALED(st))
        {
          const int sig = WTERMSIG(st);
          printRow(c,
            "*** CHILD CRASHED signal " + std::to_string(sig) +
              " (heap corruption reproduced in THIS case; backtrace above) ***");
          ++crashed;
          crashedCases.push_back(c.name);
        }
        else if (WIFEXITED(st))
        {
          switch (WEXITSTATUS(st))
          {
            case 2: ++failed; break;
            case 3: ++known; break;
            case 4: ++serialUnstable; break;
            default: break;
          }
        }
        continue;
      }
      // fork() failed -> fall through to in-process execution.
    }
#endif
    CaseOutcome oc = processCase(c, inputs, repeats, threadCounts);
    comparisons += oc.comparisons;
    printRow(c, oc.verdict);
    switch (oc.status)
    {
      case CaseStatus::Fail: ++failed; break;
      case CaseStatus::Known: ++known; break;
      case CaseStatus::SerialUnstable: ++serialUnstable; break;
      case CaseStatus::Pass: break;
    }
  }

  forceSerial(); // leave the process in a clean state

  std::cout << "\n" << std::string(84, '-') << "\n";
  std::cout << "cases: " << cases.size() << "   comparisons: " << comparisons
            << "   gated-failures: " << failed << "   known-issues (ungated): " << known
            << "   serial-unstable (ungated): " << serialUnstable << "   CRASHED: " << crashed
            << "\n";

  if (crashed > 0)
  {
    std::cout << "CRASHED CASES (heap corruption attributed by fork isolation):\n";
    for (const std::string& n : crashedCases)
      std::cout << "  - " << n << "\n";
  }

  if (failed == 0 && crashed == 0)
  {
    std::cout << "RESULT: PASS - every gated algorithm's parallel output matches serial"
              << (known || serialUnstable
                     ? " (see KNOWN-ISSUE / SERIAL-UNSTABLE rows for documented, tracked exceptions)"
                     : "")
              << "\n";
    return EXIT_SUCCESS;
  }
  std::cout << "RESULT: FAIL - " << failed << " algorithm(s) diverge, " << crashed
            << " case(s) crashed (see rows above)\n";
  return EXIT_FAILURE;
}
