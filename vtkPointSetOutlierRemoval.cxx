/*=========================================================================

  Program:   Visualization Toolkit
  Module:    $RCSfile: vtkPointSetOutlierRemoval.cxx,v $

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkPointSetOutlierRemoval.h"

#include "vtkObjectFactory.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkInformationVector.h"
#include "vtkInformation.h"

#include "vtkDoubleArray.h"
#include "vtkCellArray.h"
#include "vtkPolyData.h"
#include "vtkImageData.h"
#include "vtkDelaunay2D.h"
#include "vtkSmartPointer.h"
#include "vtkTransform.h"
#include "vtkKdTreePointLocator.h"
#include "vtkMath.h"
#include "vtkIdList.h"

#include <sstream>

vtkCxxRevisionMacro(vtkPointSetOutlierRemoval, "$Revision: 1.70 $");
vtkStandardNewMacro(vtkPointSetOutlierRemoval);

vtkPointSetOutlierRemoval::vtkPointSetOutlierRemoval()
{
  
}

vtkPointSetOutlierRemoval::~vtkPointSetOutlierRemoval()
{
}

int vtkPointSetOutlierRemoval::RequestData(vtkInformation *vtkNotUsed(request),
                                 vtkInformationVector **inputVector,
                                 vtkInformationVector *outputVector)
{
  
  // get the info object
  vtkInformation *outInfo = outputVector->GetInformationObject(0);
  
  // get the ouptut
  vtkPolyData *output = vtkPolyData::SafeDownCast(
		  outInfo->Get(vtkDataObject::DATA_OBJECT()));
  
  vtkInformation *inInfo = inputVector[0]->GetInformationObject(0);
    // get the input and ouptut
  vtkPolyData *input = vtkPolyData::SafeDownCast(
      inInfo->Get(vtkDataObject::DATA_OBJECT()));
  
  
  
  //Create the tree
  vtkSmartPointer<vtkKdTreePointLocator> KDTree = vtkSmartPointer<vtkKdTreePointLocator>::New();
  KDTree->SetDataSet(input);
  KDTree->BuildLocator();
  
  vtkstd::vector<double> Distances(input->GetNumberOfPoints());
  
  for(unsigned int i = 0; i < input->GetNumberOfPoints(); i++)
  {
    double QueryPoint[3];
    input->GetPoint(i,QueryPoint);
    
    /* can't do this - it will find exactly the same point
    vtkIdType ID = KDTree->FindClosestPoint(QueryPoint);
    double ClosestPoint[3];
    input->GetPoint(i, ClosestPoint);
    */
    
    // Find the 2 closest points to (0,0,0)
    vtkSmartPointer<vtkIdList> Result = vtkSmartPointer<vtkIdList>::New();
    KDTree->FindClosestNPoints(2, QueryPoint, Result);
  
    int point_ind = static_cast<int>(Result->GetId(1));
    double ClosestPoint[3];
    input->GetPoint(point_ind, ClosestPoint);
    
    double ClosestPointDistance = vtkMath::Distance2BetweenPoints(QueryPoint, ClosestPoint);
    Distances[i] = ClosestPointDistance;

  }
  
  //compute points to be added
  std::vector<unsigned int> Indices = ParallelSortIndices(Distances);
  
  unsigned int NumberToKeep = (1-this->PercentToRemove) * input->GetNumberOfPoints();
  //vtkstd::cout << "Number of input points: " << input->GetNumberOfPoints() << vtkstd::endl;
  //vtkstd::cout << "Percent to remove: " << this->PercentToRemove << vtkstd::endl;
  //vtkstd::cout << "Number of points to keep: " << NumberToKeep << vtkstd::endl;
  
  //add the points that should be kept to a new polydata
  vtkSmartPointer<vtkPoints> OutputPoints = vtkSmartPointer<vtkPoints>::New();
  vtkSmartPointer<vtkCellArray> OutputVertices = vtkSmartPointer<vtkCellArray>::New();
  for(unsigned int i = 0; i < NumberToKeep; i++)
  {
    //std::cout << "Dist: " << Distances[Indices[i]] << std::endl;
    double p[3];
    input->GetPoint(Indices[i], p);
    vtkIdType pid[1];
    pid[0] = OutputPoints->InsertNextPoint(p); 
    OutputVertices->InsertNextCell ( 1, pid );
  }

  //vtkstd::cout << "Number of points kept: " << OutputPoints->GetNumberOfPoints() << vtkstd::endl;
  
  vtkSmartPointer<vtkPolyData> OutputPolydata = vtkSmartPointer<vtkPolyData>::New();
  OutputPolydata->SetPoints(OutputPoints);
  OutputPolydata->SetVerts(OutputVertices);
  //vtkstd::cout << "Number of points kept: " << OutputPolydata->GetNumberOfPoints() << vtkstd::endl;
  
  output->ShallowCopy(OutputPolydata);
  
  return 1;
}


//----------------------------------------------------------------------------
void vtkPointSetOutlierRemoval::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

