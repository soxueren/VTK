/*=========================================================================

  Program:   Visualization Toolkit
  Module:    TestSimplePointsReaderWriter.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkOpenFOAMReader.h"

#include "vtkCellData.h"
#include "vtkFloatArray.h"
#include "vtkInformation.h"
#include "vtkMultiBlockDataSet.h"
#include "vtkNew.h"
#include "vtkPolyData.h"

#include "vtkTestUtilities.h"

#include <map>

int TestOpenFOAMReaderRegEx(int argc, char* argv[])
{
  // Placeholder file in the OpenFOAM case
  char* filename =
    vtkTestUtilities::ExpandDataFileName(argc, argv,
                                         "Data/OpenFOAM/regex/test.foam");

  // Read the case to get list of boundary patches and fields
  vtkNew<vtkOpenFOAMReader> reader;
  reader->SetFileName(filename);
  reader->Update();

  // Re-read again, this time with all patches (i.e. polydata)
  reader->EnableAllPatchArrays();
  reader->EnableAllCellArrays();
  reader->Update();

  // Now, this is what we expect to be read from the OpenFOAM test case.
  // Every time a field at a given patch is read and verified, its data
  // are erased from this data structure, until at the end it shoud remain
  // totally empty.
  std::map<std::string,std::map<std::string, std::vector<double>>> expected_data =
  {
      {
          "inlet",
          {
              { "p", { 0 } },
              { "U", { 1.0, 0.0, 0.0 } }
          }
      },
      {
          "outlet",
          {
              { "p", { 0 } },
              { "U", { 1.0, 0.0, 0.0 } },
          }
      },
      {
          "frontAndBack",
          {
              { "p", { 0 } },
              { "U", { 1.0, 0.0, 0.0 } }
          }
      },
      {
          "slippyWall",  // present in the test case as "*Wall" regex only
          {
              { "p", { 0 } },
              { "U", { 1.0, 0.0, 0.0 } }
          }
      },
      {
          "stickyWall",  // present in the test case explicitly
          {
              { "p", { 0 } },
              { "U", { 0.0, 0.0, 0.0 }}
          }
      }
  };

  // Print summary
  vtkMultiBlockDataSet * result = reader->GetOutput();
  if (result->GetNumberOfBlocks() > 1)
  {
    if (vtkMultiBlockDataSet * patches = vtkMultiBlockDataSet::SafeDownCast(result->GetBlock(1)))
    {
      for (int i = 0; i < patches->GetNumberOfBlocks(); i++)
      {
        // Get name of this boundary patch and fail if this patch name is not expected
        const char * patch_name = patches->GetMetaData(i)->Get(vtkCompositeDataSet::NAME());
        if (!patch_name || expected_data.find(patch_name) == expected_data.end())
            return EXIT_FAILURE;

        if (vtkPolyData * patch = vtkPolyData::SafeDownCast(patches->GetBlock(i)))
        {
          if (vtkCellData * fields = patch->GetCellData())
          {
            for (int j = 0; j < fields->GetNumberOfArrays(); j++)
            {
              if (vtkFloatArray * array = vtkFloatArray::SafeDownCast(fields->GetAbstractArray(j)))
              {
                // Get name of this boundary field and fail if this name is not expected
                const char * field_name = fields->GetArrayName(j);
                if (!field_name || expected_data[patch_name].find(field_name) == expected_data[patch_name].end())
                {
                    std::cout << "Unexpected field \"" << (field_name ? field_name : "(null)")
                              << "\" at patch \"" << patch_name << "\"" << std::endl;
                    return EXIT_FAILURE;
                }

                // Fail if the number of components read is different than expected
                const std::vector<double> & expected_components = expected_data[patch_name][field_name];
                if (expected_components.size() != array->GetNumberOfComponents())
                {
                    std::cout << "Unexpected number of components \"" << array->GetNumberOfComponents()
                              << "\" of field \"" << field_name << "\" at patch \"" << patch_name << "\"" << std::endl;
                    return EXIT_FAILURE;
                }

                // Fail if any field component is different than expected
                for (int k = 0; k < array->GetNumberOfComponents(); k++)
                {
                  if (array->GetValue(k) != expected_components[k])
                  {
                      std::cout << "Unexpected value \"" << array->GetValue(k) << "\" of component "
                                << k << " of field \"" << field_name << "\" at patch \"" << patch_name
                                << "\" (expected \"" << expected_components[k] << "\")" << std::endl;
                      return EXIT_FAILURE;
                  }
                }

                // OK, this boundary field is verified, and no longer expected -> remove
                expected_data[patch_name].erase(field_name);
              }
            }
          }
        }

        // OK, this boundary is verified -> remove
        if (expected_data[patch_name].empty())
          expected_data.erase(patch_name);
      }
    }
  }

  // By now, all expected data must have been read (and erased)
  if (!expected_data.empty())
  {
    std::cout << "FAILURE! The following data were not read:" << std::endl;
    for (auto p : expected_data)
    {
      std::cout << " patch \"" << p.first << "\"" << std::endl;
      for (auto q : p.second)
        std::cout << "   field \"" << q.first << "\"" << std::endl;
    }
  }

  return expected_data.empty() ? EXIT_SUCCESS : EXIT_FAILURE;
}
