#ifndef COLLISIONS_H
#define COLLISIONS_H

#include <vector>

#include "Tools.h"
#include "Params.h"
#include "Species.h"
#include "CollisionalIonization.h"
#include "H5.h"

class Patch;

class Collisions
{

public:
    //! Constructor for Collisions between two species
    Collisions( Patch* patch, unsigned int n_collisions, std::vector<unsigned int>,std::vector<unsigned int>, double coulomb_log, bool intra_collisions,int debug_every,int Z,bool ionizing,int nDim,double);
    //! Cloning Constructor
    Collisions(Collisions*, int);
    //! destructor
    ~Collisions();
    
    //! Method that creates a vector of Collisions objects: one for each group in the input file.
    static std::vector<Collisions*> create(Params&, Patch*, std::vector<Species*>&);
    //! Method that clones a vector of Collisions objects
    static std::vector<Collisions*> clone(std::vector<Collisions*>, Params&);
    
    //! Identification number of the Collisions object
    int n_collisions;
    
    //! Group of the species numbers that are associated for Collisions.
    std::vector<unsigned int> species_group1, species_group2;
    
    //! Coulomb logarithm (zero or negative means automatic)
    double coulomb_log;
    
    //! True if collisions inside a group of species, False if collisions between different groups of species
    bool intra_collisions;
    
    //! Number of timesteps between each dump of collisions debugging
    int debug_every;
    
    //! Method to calculate the Debye length in each cluster
    static void calculate_debye_length(Params&, Patch*);
    
    //! is true if any of the collisions objects need automatically-computed coulomb log
    static bool debye_length_required;
    
    //! Method called in the main smilei loop to apply collisions at each timestep
    void collide(Params&, Patch* ,int);
    
    //! Outputs the debug info if requested
    static void debug(Params& params, int itime, unsigned int icoll, VectorPatch& vecPatches);
    
    //! CollisionalIonization object, created if ionization required
    CollisionalIonization * Ionization;
    
private:
    
    static double cos_chi(double);
    
    int atomic_number;
    
    //! Hdf5 file name
    std::string filename;
    
    //! Temporary variables for the debugging file
    double smean, logLmean, ncol;//, temperature
};


#endif
