// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
#include "vtkMathUtilities.h"
#include "vtkStringFormatter.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <iostream>

template <class T>
bool TestSafeCastFromDouble(double input, T expected, const char* name)
{
  T output = vtkMathUtilities::SafeCastFromDouble<T>(input);
  if (output != expected)
  {
    vtk::println(
      stderr, "SafeCastFromDouble<{}>({}) returned {}, expected {}", name, input, output, expected);
    return false;
  }
  return true;
}

template <class T>
bool TestSafeCastFromDoubleToInteger(const char* name)
{
  constexpr T typeMin = std::numeric_limits<T>::lowest();
  constexpr T typeMax = std::numeric_limits<T>::max();
  constexpr double typeMinDouble = static_cast<double>(typeMin);
  constexpr double typeMaxDouble = static_cast<double>(typeMax);
  // These are for testing just over and just under the limits.
  // We go at least +1 over and at least -1 under, but we might go further
  // if a difference of just 1 at the limit is not representable by double.
  // This is only an issue for 64-bit ints, since all other integer types
  // are always exactly representable by double.
  T overTypeMin = std::max(
    static_cast<T>(typeMin + 1), static_cast<T>(std::nextafter(typeMinDouble, typeMaxDouble)));
  T underTypeMax = std::min(
    static_cast<T>(typeMax - 1), static_cast<T>(std::nextafter(typeMaxDouble, typeMinDouble)));
  double underTypeMin = std::min(
    typeMinDouble - 1.0, std::nextafter(typeMinDouble, std::numeric_limits<double>::lowest()));
  double overTypeMax = std::max(
    typeMaxDouble + 1.0, std::nextafter(typeMaxDouble, std::numeric_limits<double>::max()));

  bool success = true;

  success &= TestSafeCastFromDouble(static_cast<double>(typeMin), typeMin, name);
  success &= TestSafeCastFromDouble(static_cast<double>(typeMax), typeMax, name);
  success &= TestSafeCastFromDouble(static_cast<double>(overTypeMin), overTypeMin, name);
  success &= TestSafeCastFromDouble(static_cast<double>(underTypeMax), underTypeMax, name);
  success &= TestSafeCastFromDouble(underTypeMin, typeMin, name);
  success &= TestSafeCastFromDouble(overTypeMax, typeMax, name);
  success &= TestSafeCastFromDouble(std::numeric_limits<double>::lowest(), typeMin, name);
  success &= TestSafeCastFromDouble(std::numeric_limits<double>::max(), typeMax, name);
  success &= TestSafeCastFromDouble(-std::numeric_limits<double>::infinity(), typeMin, name);
  success &= TestSafeCastFromDouble(std::numeric_limits<double>::infinity(), typeMax, name);
  success &=
    TestSafeCastFromDouble(std::numeric_limits<double>::quiet_NaN(), static_cast<T>(0), name);

  return success;
}

template <class T>
bool TestSafeCastFromDoubleToReal(const char* name)
{
  constexpr T typeMin = std::numeric_limits<T>::lowest();
  constexpr T typeMax = std::numeric_limits<T>::max();
  T overTypeMin = static_cast<T>(std::nextafter(typeMin, typeMax));
  T underTypeMax = static_cast<T>(std::nextafter(typeMax, typeMin));

  bool success = true;

  success &= TestSafeCastFromDouble(static_cast<double>(typeMin), typeMin, name);
  success &= TestSafeCastFromDouble(static_cast<double>(typeMax), typeMax, name);
  success &= TestSafeCastFromDouble(static_cast<double>(overTypeMin), overTypeMin, name);
  success &= TestSafeCastFromDouble(static_cast<double>(underTypeMax), underTypeMax, name);
  success &= TestSafeCastFromDouble(
    -std::numeric_limits<double>::infinity(), -std::numeric_limits<T>::infinity(), name);
  success &= TestSafeCastFromDouble(
    std::numeric_limits<double>::infinity(), std::numeric_limits<T>::infinity(), name);
  // need special check since Nan != Nan
  if (!std::isnan(
        vtkMathUtilities::SafeCastFromDouble<T>(std::numeric_limits<double>::quiet_NaN())))
  {
    success &= TestSafeCastFromDouble(
      std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<T>::quiet_NaN(), name);
  }

  return success;
}

int TestMathUtilities(int, char*[])
{
  bool success = true;

  // Test SafeCastFromDouble for all VTK numeric types
  {
    vtk::println(stdout, "Testing SafeCastFromDouble");
    success &= TestSafeCastFromDoubleToInteger<char>("char");
    success &= TestSafeCastFromDoubleToInteger<signed char>("signed char");
    success &= TestSafeCastFromDoubleToInteger<unsigned char>("unsigned char");
    success &= TestSafeCastFromDoubleToInteger<short>("short");
    success &= TestSafeCastFromDoubleToInteger<unsigned short>("unsigned short");
    success &= TestSafeCastFromDoubleToInteger<int>("int");
    success &= TestSafeCastFromDoubleToInteger<unsigned int>("unsigned int");
    success &= TestSafeCastFromDoubleToInteger<long>("long");
    success &= TestSafeCastFromDoubleToInteger<unsigned long>("unsinged long");
    success &= TestSafeCastFromDoubleToInteger<long long>("long long");
    success &= TestSafeCastFromDoubleToInteger<unsigned long long>("unsigned long long");
    success &= TestSafeCastFromDoubleToReal<float>("float");
    success &= TestSafeCastFromDoubleToReal<double>("double");
  }

  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
