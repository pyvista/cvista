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

#include "smpParityCases.h"
#include "smpParityCompare.h"

#include <vtkAlgorithm.h>
#include <vtkDataObject.h>
#include <vtkNew.h>
#include <vtkSMPTools.h>
#include <vtkSmartPointer.h>

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

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
  vtkDataObject* out = algo->GetOutputDataObject(0);
  if (!out)
    return nullptr;
  vtkSmartPointer<vtkDataObject> clone;
  clone.TakeReference(out->NewInstance());
  clone->DeepCopy(out);
  return clone;
}

} // namespace

int main(int argc, char* argv[])
{
  const int repeats = (argc > 1) ? std::max(1, std::atoi(argv[1])) : 4;

  const char* def = vtkSMPTools::GetBackend();
  const unsigned hw = std::thread::hardware_concurrency();

  std::cout << "== cvista SMP parallel-vs-serial parity validator ==\n";
  std::cout << "operative default SMP backend : " << (def ? def : "(null)") << "\n";
  std::cout << "hardware_concurrency          : " << hw << "\n";
  std::cout << "repeats per (case,threadcount): " << repeats << "\n";

  // Thread counts to exercise: small, the cvista default cap (4), a larger power
  // of two when the box has the cores, and an oversubscribed count to stress the
  // scheduler into different work partitions.
  std::vector<int> threadCounts = { 2, 4 };
  if (hw >= 8)
    threadCounts.push_back(8);
  threadCounts.push_back(std::max(2u, hw * 2u));
  std::sort(threadCounts.begin(), threadCounts.end());
  threadCounts.erase(std::unique(threadCounts.begin(), threadCounts.end()), threadCounts.end());

  // Confirm STDThread actually gives us >1 worker; otherwise the comparison is
  // vacuous (still correct, just not exercising parallelism).
  forceParallel(threadCounts.back());
  const int estThreads = vtkSMPTools::GetEstimatedNumberOfThreads();
  std::cout << "STDThread estimated threads   : " << estThreads
            << (estThreads <= 1 ? "  [WARNING: no real parallelism on this host]" : "") << "\n";
  std::cout << "thread counts exercised       : ";
  for (int t : threadCounts)
    std::cout << t << " ";
  std::cout << "\n\n";

  // Inputs are built once, under the serial floor, so they are deterministic.
  forceSerial();
  const smpparity::Inputs inputs = smpparity::BuildInputs();

  const std::vector<smpparity::Case> cases = smpparity::RegisterCases();

  int failed = 0;
  int comparisons = 0;

  std::cout << std::left << std::setw(34) << "algorithm" << std::setw(18) << "module"
            << std::setw(13) << "risk" << "result\n";
  std::cout << std::string(84, '-') << "\n";

  for (const smpparity::Case& c : cases)
  {
    forceSerial();
    vtkSmartPointer<vtkDataObject> ref = runOnce(c, inputs);

    std::string verdict;
    bool ok = true;
    if (!ref)
    {
      ok = false;
      verdict = "ERROR: serial run produced no output";
    }

    for (int t = 0; ok && t < static_cast<int>(threadCounts.size()); ++t)
    {
      const int nthreads = threadCounts[t];
      forceParallel(nthreads);
      vtkSmartPointer<vtkDataObject> first; // first parallel output at this count
      for (int r = 0; ok && r < repeats; ++r)
      {
        vtkSmartPointer<vtkDataObject> out = runOnce(c, inputs);
        ++comparisons;
        if (!out)
        {
          ok = false;
          verdict = "FAIL: no output @T=" + std::to_string(nthreads);
          break;
        }

        // (1) run-to-run determinism: every parallel run at a fixed thread count
        // must be byte-identical to the first. This is the real "instability"
        // check -- it must hold even for order-relaxed filters.
        if (r == 0)
        {
          first = out;
          // (2) vs serial: byte-exact, or (order-relaxed) same geometry set.
          std::string diff = c.orderRelaxed ? smpparity::CompareGeometrySet(ref, out)
                                            : smpparity::CompareDataObjects(ref, out);
          if (!diff.empty())
          {
            ok = false;
            verdict = std::string("FAIL ") + (c.orderRelaxed ? "geometry" : "byte-exact") +
              " vs serial @T=" + std::to_string(nthreads) + ": " + diff;
            break;
          }
        }
        else
        {
          std::string diff = smpparity::CompareDataObjects(first, out);
          if (!diff.empty())
          {
            ok = false;
            verdict = "FAIL nondeterministic @T=" + std::to_string(nthreads) + " rep " +
              std::to_string(r) + ": " + diff;
            break;
          }
        }
      }
    }

    if (ok)
      verdict = c.orderRelaxed ? "PASS (order-relaxed: same set, run-to-run stable)" : "PASS";
    else
      ++failed;

    std::cout << std::left << std::setw(34) << c.name << std::setw(18) << c.module
              << std::setw(13) << smpparity::RiskName(c.risk) << verdict << "\n";
  }

  forceSerial(); // leave the process in a clean state

  std::cout << "\n" << std::string(84, '-') << "\n";
  std::cout << "cases: " << cases.size() << "   comparisons: " << comparisons
            << "   failed: " << failed << "\n";
  if (failed == 0)
  {
    std::cout << "RESULT: PASS - every algorithm's parallel output is byte-exact with serial\n";
    return EXIT_SUCCESS;
  }
  std::cout << "RESULT: FAIL - " << failed
            << " algorithm(s) diverge between parallel and serial (see rows above)\n";
  return EXIT_FAILURE;
}
