#include <vtkSmartPointer.h>
#include <vtkObjectFactory.h>
#include <vtkIdList.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkDoubleArray.h>
#include <vtkPointData.h>
#include <vtkInformation.h>
#include <vtkInformationVector.h>
#include <vtkPlane.h>
#include <vtkPolyData.h>
#include <vtkKdTree.h>
#include <vtkMath.h>

#include "vtkPointSetNormalEstimation.h"

vtkCxxRevisionMacro(vtkPointSetNormalEstimation, "$Revision: 1.70 $");
vtkStandardNewMacro(vtkPointSetNormalEstimation);

vtkPointSetNormalEstimation::vtkPointSetNormalEstimation()
{
  kNeighbors = 4;
}

int vtkPointSetNormalEstimation::RequestData(vtkInformation *vtkNotUsed(request),
                                 vtkInformationVector **inputVector,
                                 vtkInformationVector *outputVector)
{
  // get the info objects
  vtkInformation *inInfo = inputVector[0]->GetInformationObject(0);
  vtkInformation *outInfo = outputVector->GetInformationObject(0);

  // get the input and ouptut
  vtkPolyData *input = vtkPolyData::SafeDownCast(
      inInfo->Get(vtkDataObject::DATA_OBJECT()));
  vtkPolyData *output = vtkPolyData::SafeDownCast(
      outInfo->Get(vtkDataObject::DATA_OBJECT()));

  //create normal array
  vtkSmartPointer<vtkDoubleArray> normalArray = 
      vtkSmartPointer<vtkDoubleArray>::New();
  normalArray->SetNumberOfComponents( 3 );
  normalArray->SetNumberOfTuples( input->GetNumberOfPoints() );
  normalArray->SetName( "Normals" );
  
  vtkSmartPointer<vtkKdTree> kDTree = 
      vtkSmartPointer<vtkKdTree>::New();
  kDTree->BuildLocatorFromPoints(input->GetPoints());

  //add a random vector (clearly to be replaced with your actual normals) to each point in the polydata
  for(unsigned int i = 0; i < input->GetNumberOfPoints(); i++)
    {
    
    double point[3];
    input->GetPoint(i, point);

    vtkSmartPointer<vtkIdList> neighborIds = 
        vtkSmartPointer<vtkIdList>::New();
    kDTree->FindClosestNPoints(this->kNeighbors, point, neighborIds);
    
    vtkSmartPointer<vtkPoints> neighbors = 
        vtkSmartPointer<vtkPoints>::New();
    for(unsigned int p = 0; p < neighborIds->GetNumberOfIds(); p++)
      {
      double neighbor[3];
      input->GetPoint(neighborIds->GetId(p), neighbor);
      neighbors->InsertNextPoint(neighbor);
      }
    
    vtkSmartPointer<vtkPlane> bestPlane = 
        vtkSmartPointer<vtkPlane>::New();
    BestFitPlane(neighbors, bestPlane);
    double normal[3];
    bestPlane->GetNormal(normal);
    normalArray->SetTuple( i, normal ) ;
    }
 
  input->GetPointData()->SetNormals(normalArray);

  output->ShallowCopy(input);
  
  return 1;
}

////////// External Operators /////////////

void vtkPointSetNormalEstimation::PrintSelf(ostream &os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
  os << indent << "kNeighbors: " << this->kNeighbors << endl;
}

void CenterOfMass(vtkPoints* points, double* center)
{
  //Compute the center of mass of a set of points.
  center[0] = 0.0;
  center[1] = 0.0;
  center[2] = 0.0;
    
  for(vtkIdType i = 0; i < points->GetNumberOfPoints(); i++)
    {
    double point[3];
    points->GetPoint(i, point);
    
    center[0] += point[0];
    center[1] += point[1];
    center[2] += point[2];
    }
  
  double numberOfPoints = static_cast<double>(points->GetNumberOfPoints());
  center[0] = center[0]/numberOfPoints;
  center[1] = center[1]/numberOfPoints;
  center[2] = center[2]/numberOfPoints;
}

/* allocate memory for an nrow x ncol matrix */
template<class TReal>
    TReal **create_matrix ( long nrow, long ncol )
{
  typedef TReal* TRealPointer;
  TReal **m = new TRealPointer[nrow];

  TReal* block = ( TReal* ) calloc ( nrow*ncol, sizeof ( TReal ) );
  m[0] = block;
  for ( int row = 1; row < nrow; ++row )
    {
    m[ row ] = &block[ row * ncol ];
    }
  return m;
}

/* free a TReal matrix allocated with matrix() */
template<class TReal>
    void free_matrix ( TReal **m )
{
  free ( m[0] );
  delete[] m;
}

void BestFitPlane(vtkPoints *points, vtkPlane *bestPlane)
{
  //Compute the best fit (least squares) plane through a set of points.
  vtkIdType numPoints = points->GetNumberOfPoints();
  double dNumPoints = static_cast<double>(numPoints);
  
  //find the center of mass of the points
  double center[3];
  CenterOfMass(points, center);
  //vtkstd::cout << "Center of mass: " << Center[0] << " " << Center[1] << " " << Center[2] << vtkstd::endl;
  
  //Compute sample covariance matrix
  double **a = create_matrix<double> ( 3,3 );
  a[0][0] = 0; a[0][1] = 0;  a[0][2] = 0;
  a[1][0] = 0; a[1][1] = 0;  a[1][2] = 0;
  a[2][0] = 0; a[2][1] = 0;  a[2][2] = 0;
  
  for(unsigned int pointId = 0; pointId < numPoints; pointId++ )
    {
    double x[3];
    double xp[3];
    points->GetPoint(pointId, x);
    xp[0] = x[0] - center[0]; 
    xp[1] = x[1] - center[1]; 
    xp[2] = x[2] - center[2];
    for (unsigned int i = 0; i < 3; i++)
      {
      a[0][i] += xp[0] * xp[i];
      a[1][i] += xp[1] * xp[i];
      a[2][i] += xp[2] * xp[i];
      }
    }
  
    //divide by N-1
  for(unsigned int i = 0; i < 3; i++)
    {
    a[0][i] /= dNumPoints-1;
    a[1][i] /= dNumPoints-1;
    a[2][i] /= dNumPoints-1;
    }

  // Extract eigenvectors from covariance matrix
  double **eigvec = create_matrix<double> ( 3,3 );
  
  double eigval[3];
  vtkMath::Jacobi(a,eigval,eigvec);

    //Jacobi iteration for the solution of eigenvectors/eigenvalues of a 3x3 real symmetric matrix. Square 3x3 matrix a; output eigenvalues in w; and output eigenvectors in v. Resulting eigenvalues/vectors are sorted in decreasing order; eigenvectors are normalized.
  
  //Set the plane normal to the smallest eigen vector
  bestPlane->SetNormal(eigvec[0][2], eigvec[1][2], eigvec[2][2]);
  
  //cleanup
  free_matrix(eigvec);
  free_matrix(a);
  
  //Set the plane origin to the center of mass
  bestPlane->SetOrigin(center[0], center[1], center[2]);

}
