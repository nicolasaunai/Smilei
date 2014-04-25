#include "Field3D.h"

#include <iostream>
#include <vector>

using namespace std;



// ---------------------------------------------------------------------------------------------------------------------
// Creators for Field3D
// ---------------------------------------------------------------------------------------------------------------------

// with no input argument
Field3D::Field3D() : Field()
{
    data_=NULL;
}

// with the dimensions as input argument
Field3D::Field3D(vector<unsigned int> dims) : Field(dims)
{
    data_=NULL;
    allocateDims(dims);
}

// with the dimensions and output (dump) file name as input argument
Field3D::Field3D(vector<unsigned int> dims, string name) : Field(dims, name)
{
    allocateDims(dims);
}

// with the dimensions as input argument
Field3D::Field3D(vector<unsigned int> dims, unsigned int mainDim, bool isPrimal) : Field(dims, mainDim, isPrimal)
{
    data_=NULL;
    allocateDims(dims, mainDim, isPrimal);
}

// with the dimensions and output (dump) file name as input argument
Field3D::Field3D(vector<unsigned int> dims, unsigned int mainDim, bool isPrimal, string name) : Field(dims, mainDim, isPrimal, name)
{
    allocateDims(dims, mainDim, isPrimal);
}


// ---------------------------------------------------------------------------------------------------------------------
// Destructor for Field3D
// ---------------------------------------------------------------------------------------------------------------------
Field3D::~Field3D()
{
    delete [] data_;
    for (unsigned int i=0; i<dims_[0]; i++) delete [] data_3D[i];
    delete [] data_3D;

}


// ---------------------------------------------------------------------------------------------------------------------
// Method used for allocating the dimension of a Field3D
// ---------------------------------------------------------------------------------------------------------------------
void Field3D::allocateDims(std::vector<unsigned int> dims ) {
    dims_=dims;
    if (dims_.size()!=3) ERROR("Alloc error must be 3 : " << dims.size());
    if (data_) delete [] data_;

    isDual_.resize( dims.size(), 0 );

    data_ = new double[dims_[0]*dims_[1]*dims_[2]];
    //! \todo{check row major order!!!}
    data_3D= new double**[dims_[0]*dims_[1]];
    for (unsigned int i=0; i<dims_[0]; i++)
    {
        data_3D[i]= new double*[dims_[1]];
        for (unsigned int j=0; j<dims_[1]; j++)
        {
            data_3D[i][j] = data_ + i*dims_[1]*dims_[2] + j*dims_[2];
        }
    }//i

    //DEBUG(10,"Fields 3D created: " << dims_[0] << "x" << dims_[1] << "x" << dims_[2]);
    globalDims_ = dims_[0]*dims_[1]*dims_[2];

}


void Field3D::allocateDims(unsigned int dims1, unsigned int dims2, unsigned int dims3)
{
	vector<unsigned int> dims(3);
	dims[0]=dims1;
	dims[1]=dims2;
	dims[2]=dims3;
	allocateDims(dims);
}


// ---------------------------------------------------------------------------------------------------------------------
// Method used for allocating the dimension of a Field3D
// ---------------------------------------------------------------------------------------------------------------------
void Field3D::allocateDims(std::vector<unsigned int> dims, unsigned int mainDim, bool isPrimal ) {
    dims_=dims;
    if (dims_.size()!=3) ERROR("Alloc error must be 3 : " << dims.size());
    if (data_) delete [] data_;

    // isPrimal define if mainDim is Primal or Dual
    isDual_.resize( dims.size(), 0 );
    for ( unsigned int j=0 ; j<dims.size() ; j++ ) {
        if ( (j==mainDim) && (!isPrimal) )
            isDual_[j] = 1;
        else if ( (j!=mainDim) && (isPrimal) )
            isDual_[j] = 1;
    }

    for ( unsigned int j=0 ; j<dims.size() ; j++ )
        dims_[j] += isDual_[j];

    data_ = new double[dims_[0]*dims_[1]*dims_[2]];
    //! \todo{check row major order!!!}
    data_3D= new double**[dims_[0]*dims_[1]];
    for (unsigned int i=0; i<dims_[0]; i++)
    {
        data_3D[i]= new double*[dims_[1]];
        for (unsigned int j=0; j<dims_[1]; j++)
        {
            data_3D[i][j] = data_ + i*dims_[1]*dims_[2] + j*dims_[2];
        }
    }//i

    //DEBUG(10,"Fields 3D created: " << dims_[0] << "x" << dims_[1] << "x" << dims_[2]);
    globalDims_ = dims_[0]*dims_[1]*dims_[2];

    //isDual_ = isPrimal;
}



// ---------------------------------------------------------------------------------------------------------------------
// Method used to create a dump for a Field3D
// ---------------------------------------------------------------------------------------------------------------------
void Field3D::dump(vector<unsigned int> dims)
{

}
