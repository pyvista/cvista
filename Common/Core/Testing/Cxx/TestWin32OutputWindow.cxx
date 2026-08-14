// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkOutputWindow.h"

#include "vtkStringFormatter.h"
#include "vtkStringScanner.h"
#include "vtkWindows.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include <vtksys/SystemTools.hxx>

int TestWin32OutputWindow(int argc, char* argv[])
{
  (void)argc;
  (void)argv;
  const auto numThreads = std::thread::hardware_concurrency();
  std::vector<std::thread> threads;
  threads.reserve(numThreads);
  for (unsigned int i = 0; i < numThreads; ++i)
  {
    threads.emplace_back(
      [i]()
      {
        std::string message = vtk::to_string(i) + "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        vtkOutputWindow::GetInstance()->DisplayWarningText(message.c_str());
      });
  }
  for (auto& thread : threads)
  {
    thread.join();
  }
  // Give time for messages to settle in the output window UI thread
  std::this_thread::sleep_for(std::chrono::seconds(2));
  // After threads join:
  HWND parent = FindWindowA("vtkOutputWindow", nullptr);
  HWND edit = FindWindowExA(parent, nullptr, "EDIT", nullptr);
  int len = GetWindowTextLengthA(edit);
  std::string content(len, '\0');
  GetWindowTextA(edit, content.data(), len + 1);
  // Verify that all threads had a chance to display
  const auto lines = vtksys::SystemTools::SplitString(content, '\n');
  std::vector<bool> seen(numThreads, false);
  for (auto& line : lines)
  {
    if (line.empty())
    {
      continue;
    }
    int i = 0;
    VTK_FROM_CHARS_IF_ERROR_RETURN(line, i, EXIT_FAILURE);
    seen[i] = true;
  }
  bool success = true;
  for (std::size_t i = 0; i < seen.size(); ++i)
  {
    if (!seen[i])
    {
      std::cerr << "Thread " << i << " message was not displayed!\n";
      success &= false;
    }
  }

  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
