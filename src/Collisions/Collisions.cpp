
#include <cmath>
#include <iomanip>
#include <algorithm>
#include <ostream>
#include <fstream>

#include "Collisions.h"
#include "SmileiMPI.h"
#include "Field2D.h"
#include "H5.h"
#include "Patch.h"
#include "VectorPatch.h"

using namespace std;


// Constructor
Collisions::Collisions(
    Patch* patch,
    unsigned int n_collisions,
    vector<unsigned int> species_group1, 
    vector<unsigned int> species_group2, 
    double coulomb_log, 
    bool intra_collisions,
    int debug_every,
    int Z,
    bool ionizing,
    int nDim,
    double referenceAngularFrequency_SI
) :
n_collisions    (n_collisions    ),
species_group1  (species_group1  ),
species_group2  (species_group2  ),
coulomb_log     (coulomb_log     ),
intra_collisions(intra_collisions),
debug_every     (debug_every     ),
atomic_number   (Z               ),
filename("")
{
    // Create the ionization object
    if( ionizing ) {
        Ionization = new CollisionalIonization(Z, nDim, referenceAngularFrequency_SI);
    } else {
        Ionization = new CollisionalNoIonization();
    }
    
    // If debugging log requested
    if( debug_every>0 ) {
        // Build the file name
        ostringstream mystream("");
        mystream << "Collisions" << n_collisions << ".h5";
        filename = mystream.str();
    }
}


// Cloning Constructor
Collisions::Collisions( Collisions* coll, int nDim )
{

    n_collisions     = coll->n_collisions    ;
    species_group1   = coll->species_group1  ;
    species_group2   = coll->species_group2  ;
    coulomb_log      = coll->coulomb_log     ;
    intra_collisions = coll->intra_collisions;
    debug_every      = coll->debug_every     ;
    atomic_number    = coll->atomic_number   ;
    filename         = coll->filename        ;
    
    if( atomic_number>0 ) {
        Ionization = new CollisionalIonization(coll->Ionization);
    } else {
        Ionization = new CollisionalNoIonization();
    }
}


Collisions::~Collisions()
{
    delete Ionization;
}


// Reads the input file and creates the Collisions objects accordingly
vector<Collisions*> Collisions::create(Params& params, Patch* patch, vector<Species*>& vecSpecies)
{
    vector<Collisions*> vecCollisions;
    
    vector<string> sg1, sg2;
    vector<vector<unsigned int>> sgroup;
    double clog;
    bool intra, debye_length_required = false, ionizing;
    int debug_every, Z, Z0, Z1;
    ostringstream mystream;
    Species *s0, *s;
    
    // Needs referenceAngularFrequency_SI to be defined
    unsigned int numcollisions=PyTools::nComponents("Collisions");
    if (numcollisions > 0)
        if (params.referenceAngularFrequency_SI <= 0.)
            ERROR("The parameter `referenceAngularFrequency_SI` needs to be defined and positive to compute collisions");
    
    // Loop over each binary collisions group and parse info
    for (unsigned int n_collisions = 0; n_collisions < numcollisions; n_collisions++) {
        
        MESSAGE(1,"Parameters for collisions #" << n_collisions << " :");
        
        // Read the input file by searching for the keywords "species1" and "species2"
        // which are the names of the two species that will collide
        sg1.resize(0);
        sg2.resize(0);
        PyTools::extract("species1",sg1,"Collisions",n_collisions);
        PyTools::extract("species2",sg2,"Collisions",n_collisions);
        
        // Obtain the lists of species numbers from the lists of species names.
        sgroup.resize(2);
        sgroup[0] = params.FindSpecies(vecSpecies, sg1);
        sgroup[1] = params.FindSpecies(vecSpecies, sg2);
        
        // Each group of species sgroup[0] and sgroup[1] must not be empty
        if (sgroup[0].size()==0) ERROR("In collisions #" << n_collisions << ": No valid `species1`");
        if (sgroup[1].size()==0) ERROR("In collisions #" << n_collisions << ": No valid `species2`");
        
        // sgroup[0] and sgroup[1] can be equal, but cannot have common species if they are not equal
        if (sgroup[0] != sgroup[1]) {
            for (unsigned int i0=0; i0<sgroup[0].size(); i0++) {
                for (unsigned int i1=0; i1<sgroup[1].size(); i1++) {
                    if (sgroup[0][i0] == sgroup[1][i1])
                        ERROR("In collisions #" << n_collisions << ": species #" << sgroup[0][i0]
                            << " cannot collide with itself");
                }
            }
            intra = false;
        } else {
            intra = true;
        }
        
        // Coulomb logarithm (if negative or unset, then automatically computed)
        clog = 0.; // default
        PyTools::extract("coulomb_log",clog,"Collisions",n_collisions);
        if (clog <= 0.) debye_length_required = true; // auto coulomb log requires debye length
        
        // Number of timesteps between each debug output (if 0 or unset, no debug)
        debug_every = 0; // default
        PyTools::extract("debug_every",debug_every,"Collisions",n_collisions);
        
        // Collisional ionization
        ionizing = false; Z = 0; // default
        PyTools::extract("ionizing",ionizing,"Collisions",n_collisions);
        if( ionizing ) {
            if( intra )
                ERROR("In collisions #" << n_collisions << ": cannot ionize with intra-collisions");
            
            for (int g=0; g<2; g++) { // do sgroup[0], then sgroup[1]
                s0 = vecSpecies[sgroup[g][0]]; // first species of this group
                for (unsigned int i=1; i<sgroup[g].size(); i++) { // loop other species of same group
                    s = vecSpecies[sgroup[g][i]]; // current species
                    if( s->mass != s0->mass )
                        ERROR("In collisions #" << n_collisions << ": species in group `species"
                            << g+1 << "` must all have same masses for ionization");
                    
                    if( s->atomic_number != s0->atomic_number ) {
                        if( s->atomic_number * s0->atomic_number ==0 ) {
                            ERROR("In collisions #" << n_collisions << ": species in group `species"
                            << g+1 << "` cannot be mixed electrons and ions for ionization");
                        } else {
                            ERROR("In collisions #" << n_collisions << ": species in group `species"
                            << g+1 << "` must all have same atomic_number for ionization");
                        }
                    }
                }
            }
            // atomic number
            Z0 = vecSpecies[sgroup[0][0]]->atomic_number;
            Z1 = vecSpecies[sgroup[1][0]]->atomic_number;
            Z = (int) ( Z0>Z1 ? Z0 : Z1 );
            if( Z0*Z1!=0 )
                ERROR("In collisions #" << n_collisions << ": ionization requires electrons (no or null atomic_number)");
            if( Z==0 )
                ERROR("In collisions #" << n_collisions << ": ionization requires ions (atomic_number>0)");
        }
        
        // Print collisions parameters
        mystream.str(""); // clear
        mystream << "(" << sgroup[0][0];
        for (unsigned int rs=1 ; rs<sgroup[0].size() ; rs++) mystream << " " << sgroup[0][rs];
        if( intra ) {
            MESSAGE(2,"Intra collisions within species " << mystream.str() << ")");
        } else {
            mystream << ") and (" << sgroup[1][0];
            for (unsigned int rs=1 ; rs<sgroup[1].size() ; rs++) mystream << " " << sgroup[1][rs];
            MESSAGE(2,"Collisions between species " << mystream.str() << ")");
        }
        MESSAGE(2,"Coulomb logarithm: " << clog);
        if( debug_every>0 ) MESSAGE(2,"Debug every " << debug_every << " timesteps");
        mystream.str(""); // clear
        if( ionizing>0 ) MESSAGE(2,"Collisional ionization with atomic number "<<Z);
        
        // Add new Collisions objects to vector
        vecCollisions.push_back(
            new Collisions(
                patch,
                n_collisions,
                sgroup[0],
                sgroup[1],
                clog, intra,
                debug_every,
                Z,
                ionizing,
                params.nDim_particle,
                params.referenceAngularFrequency_SI
            )
        );
        
        // Create debug file
        if( debug_every>0 ) {
            string filename = vecCollisions.back()->filename;
            ifstream file(filename);
            // Check if file exists
            if (! file) {
                // Create the file access protocol for writing in the debug file later
                hid_t file_access = H5Pcreate(H5P_FILE_ACCESS);
                H5Pset_fapl_mpio(file_access, MPI_COMM_WORLD, MPI_INFO_NULL);
                // Create file
                hid_t fileId = H5Fcreate(filename.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, file_access);
                H5Pclose(file_access);
                // write all parameters as HDF5 attributes
                H5::attr(fileId, "Version", string(__VERSION));
                ostringstream mystream("");
                mystream << sgroup[0][0];
                for(unsigned int i=1; i<sgroup[0].size(); i++) mystream << "," << sgroup[0][i];
                H5::attr(fileId, "species1" , mystream.str());
                mystream.str("");
                mystream << sgroup[1][0];
                for(unsigned int i=1; i<sgroup[1].size(); i++) mystream << "," << sgroup[1][i];
                H5::attr(fileId, "species2" , mystream.str());
                H5::attr(fileId, "coulomb_log" , clog);
                H5::attr(fileId, "debug_every"  , debug_every);
                H5Fclose(fileId);
            }
        }
        
    }
    
    // pass the variable "debye_length_required" into the Collision class
    Collisions::debye_length_required = debye_length_required;
    
    return vecCollisions;
}


// Clone a vector of Collisions objects
vector<Collisions*> Collisions::clone(vector<Collisions*> vecCollisions, Params& params)
{
    vector<Collisions*> newVecCollisions(0);
    
    for( unsigned int i=0; i<vecCollisions.size(); i++)
        newVecCollisions.push_back( new Collisions( vecCollisions[i], params.nDim_particle ) );
    
    return newVecCollisions;
}


// Declare other static variables here
bool   Collisions::debye_length_required;



// Calculates the debye length squared in each patch
// The formula for the inverse debye length squared is sumOverSpecies(density*charge^2/temperature)
void Collisions::calculate_debye_length(Params& params, Patch * patch)
{

    unsigned int nspec = patch->vecSpecies.size(); // number of species
    double p2, density, density_max, charge, temperature, rmin2;
    Species   * s;
    Particles * p;
    double coeff = 299792458./(3.*params.referenceAngularFrequency_SI*2.8179403267e-15); // c / (3 omega re)
    
    density_max = 0.;
    patch->debye_length_squared = 0.;
    for (unsigned int ispec=0 ; ispec<nspec ; ispec++) { // loop all species
        // Calculation of particles density, mean charge, and temperature
        // Density is the sum of weights
        // Temperature definition is the average <v*p> divided by 3
        //    (instead of v*p, we use p^2/gamma)
        s  = patch->vecSpecies[ispec];
        p  = (s->particles);
        density     = 0.;
        charge      = 0.;
        temperature = 0.;
        // loop particles to calculate average quantities
        for (unsigned int iPart=0 ; iPart<p->size() ; iPart++ ) {
            p2 = pow(p->momentum(0,iPart),2)+pow(p->momentum(1,iPart),2)+pow(p->momentum(2,iPart),2);
            density     += p->weight(iPart);
            charge      += p->weight(iPart) * p->charge(iPart);
            temperature += p->weight(iPart) * p2/sqrt(1.+p2);
        }
        if (density <= 0.) continue;
        charge /= density; // average charge
        temperature *= (s->mass) / (3.*density); // Te in units of me*c^2
        density /= (double)params.n_cell_per_patch; // density in units of critical density
        // compute inverse debye length squared
        if (temperature>0.) patch->debye_length_squared += density*charge*charge/temperature;
        // compute maximum density of species
        if (density>density_max) density_max = density;
    }
    
    // if there were particles, 
    if (patch->debye_length_squared > 0.) {
        // compute debye length squared in code units
        patch->debye_length_squared = 1./(patch->debye_length_squared);
        // apply lower limit to the debye length (minimum interatomic distance)
        rmin2 = pow(coeff*density_max, -2./3.);
        if (patch->debye_length_squared < rmin2) patch->debye_length_squared = rmin2;
    }
    
#ifdef  __DEBUG
    // calculate and print average debye length
    double mean_debye_length = sqrt(patch->debye_length_squared);
    //DEBUG("Mean Debye length in code length units = " << scientific << setprecision(3) << mean_debye_length);
    mean_debye_length *= 299792458./params.referenceAngularFrequency_SI; // switch to SI
    DEBUG("Mean Debye length in meters = " << scientific << setprecision(3) << mean_debye_length );
#endif

}

// Calculates the collisions for a given Collisions object
void Collisions::collide(Params& params, Patch* patch, int itime)
{

    vector<unsigned int> *sg1, *sg2, *sgtmp, index1, index2;
    unsigned int nspec1, nspec2; // numbers of species in each group
    unsigned int npart1, npart2; // numbers of macro-particles in each group
    unsigned int npairs; // number of pairs of macro-particles
    vector<unsigned int> np1, np2; // numbers of macro-particles in each species, in each group
    double n1, n2, n12, n123, n223; // densities of particles
    unsigned int i1, i2, ispec1, ispec2, N2max;
    Species   *s1, *s2;
    Particles *p1, *p2;
    double m1, m2, m12, W1, W2, qqm, qqm2, gamma1, gamma2, gamma12, gamma12_inv,
           COM_vx, COM_vy, COM_vz, COM_vsquare, COM_gamma,
           term1, term2, term3, term4, term5, term6, coeff1, coeff2, coeff3, coeff4, twoPi,
           vcv1, vcv2, px_COM, py_COM, pz_COM, p2_COM, p_COM, gamma1_COM, gamma2_COM,
           logL, bmin, s, vrel, smax,
           cosX, sinX, phi, sinXcosPhi, sinXsinPhi, p_perp, inv_p_perp, 
           newpx_COM, newpy_COM, newpz_COM, U, vcp, n_patch_per_cell;
    bool not_duplicated_particle;
    
    sg1 = &species_group1;
    sg2 = &species_group2;
    
    
    bool debug = (debug_every > 0 && itime % debug_every == 0); // debug only every N timesteps
    
    if( debug ) {
        smean       = 0.;
        logLmean    = 0.;
        //temperature = 0.;
    }
    
    // Initialize some stuff
    twoPi = 2. * M_PI;
    coeff1 = 4.046650232e-21*params.referenceAngularFrequency_SI; // h*omega/(2*me*c^2)
    coeff2 = 2.817940327e-15*params.referenceAngularFrequency_SI/299792458.; // re omega / c
    n_patch_per_cell = 1./((double)params.n_cell_per_patch);
    
    // get number of particles for all necessary species
    for (unsigned int i=0; i<2; i++) { // try twice to ensure group 1 has more macro-particles
        nspec1 = sg1->size();
        nspec2 = sg2->size();
        np1.resize(nspec1); // number of particles in each species of group 1
        np2.resize(nspec2); // number of particles in each species of group 2
        npart1 = 0;
        npart2 = 0;
        for (ispec1=0 ; ispec1<nspec1 ; ispec1++) {
            np1[ispec1] = patch->vecSpecies[(*sg1)[ispec1]]->particles->size();
            npart1 += np1[ispec1];
        }
        for (ispec2=0 ; ispec2<nspec2 ; ispec2++) {
            np2[ispec2] = patch->vecSpecies[(*sg2)[ispec2]]->particles->size();
            npart2 += np2[ispec2];
        }
        if (npart2 <= npart1) break; // ok if group1 has more macro-particles
        else { // otherwise, we exchange groups and try again
            sgtmp = sg1; sg1 = sg2; sg2 = sgtmp;
        }
    }
    // now group1 has more macro-particles than group2
    
    // skip to next bin if no particles
    if (npart1==0 || npart2==0) return;
    
    // Shuffle particles to have random pairs
    //    (It does not really exchange them, it is just a temporary re-indexing)
    index1.resize(npart1);
    for (unsigned int i=0; i<npart1; i++) index1[i] = i; // first, we make an ordered array
    random_shuffle(index1.begin(), index1.end()); // shuffle the index array
    if (intra_collisions) { // In the case of collisions within one species
        npairs = (int) ceil(((double)npart1)/2.); // half as many pairs as macro-particles
        index2.resize(npairs);
        for (unsigned int i=0; i<npairs; i++) index2[i] = index1[(i+npairs)%npart1]; // index2 is second half
        index1.resize(npairs); // index1 is first half
        N2max = npart1 - npairs; // number of not-repeated particles (in group 2 only)
    } else { // In the case of collisions between two species
        npairs = npart1; // as many pairs as macro-particles in group 1 (most numerous)
        index2.resize(npairs);
        for (unsigned int i=0; i<npart1; i++) index2[i] = i % npart2;
        N2max = npart2; // number of not-repeated particles (in group 2 only)
    }
    
    // Prepare the ionization
    Ionization->prepare1(patch->vecSpecies[(*sg1)[0]]->atomic_number);
    
    // Calculate the densities
    n1  = 0.; // density of group 1
    n2  = 0.; // density of group 2
    n12 = 0.; // "hybrid" density
    for (unsigned int i=0; i<npairs; i++) { // for each pair of particles
        // find species and index i1 of particle "1"
        i1 = index1[i];
        for (ispec1=0 ; i1>=np1[ispec1]; ispec1++) i1 -= np1[ispec1];
        // find species and index i2 of particle "2"
        not_duplicated_particle = (i<N2max);
        i2 = index2[i];
        for (ispec2=0 ; i2>=np2[ispec2]; ispec2++) i2 -= np2[ispec2];
        // Pointers to particles
        p1 = patch->vecSpecies[(*sg1)[ispec1]]->particles;
        p2 = patch->vecSpecies[(*sg2)[ispec2]]->particles;
        // sum weights
        n1  += p1->weight(i1);
        if( not_duplicated_particle ) n2  += p2->weight(i2); // special case for group 2 to avoid repeated particles
        n12 += min( p1->weight(i1), p2->weight(i2) );
        // Same for ionization
        Ionization->prepare2(p1, i1, p2, i2, not_duplicated_particle);
    }
    if( intra_collisions ) { n1 += n2; n2 = n1; }
    n1  *= n_patch_per_cell;
    n2  *= n_patch_per_cell;
    n12 *= n_patch_per_cell;
    
    // Pre-calculate some numbers before the big loop
    n123 = pow(n1,2./3.);
    n223 = pow(n2,2./3.);
    coeff3 = params.timestep * n1*n2/n12;
    coeff4 = pow( 3.*coeff2 , -1./3. ) * coeff3;
    coeff3 *= coeff2;
    
    // Prepare the ionization
    Ionization->prepare3(params.timestep, n_patch_per_cell);
    
    // Now start the real loop on pairs of particles
    // See equations in http://dx.doi.org/10.1063/1.4742167
    // ----------------------------------------------------
    for (unsigned int i=0; i<npairs; i++) {
    
        // find species and index i1 of particle "1"
        i1 = index1[i];
        for (ispec1=0 ; i1>=np1[ispec1]; ispec1++) i1 -= np1[ispec1];
        // find species and index i2 of particle "2"
        i2 = index2[i];
        for (ispec2=0 ; i2>=np2[ispec2]; ispec2++) i2 -= np2[ispec2];
        
        s1 = patch->vecSpecies[(*sg1)[ispec1]]; s2 = patch->vecSpecies[(*sg2)[ispec2]];
        p1 = s1->particles;                     p2 = s2->particles;
        m1 = s1->mass;                          m2 = s2->mass;
        W1 = p1->weight(i1);                    W2 = p2->weight(i2);
        
        // Calculate stuff
        m12  = m1 / m2; // mass ratio
        qqm  = p1->charge(i1) * p2->charge(i2) / m1;
        qqm2 = qqm * qqm;
        
        // Get momenta and calculate gammas
        gamma1 = sqrt(1. + pow(p1->momentum(0,i1),2) + pow(p1->momentum(1,i1),2) + pow(p1->momentum(2,i1),2));
        gamma2 = sqrt(1. + pow(p2->momentum(0,i2),2) + pow(p2->momentum(1,i2),2) + pow(p2->momentum(2,i2),2));
        gamma12 = m12 * gamma1 + gamma2;
        gamma12_inv = 1./gamma12;
        
        // Calculate the center-of-mass (COM) frame
        // Quantities starting with "COM" are those of the COM itself, expressed in the lab frame.
        // They are NOT quantities relative to the COM.
        COM_vx = ( m12 * (p1->momentum(0,i1)) + p2->momentum(0,i2) ) * gamma12_inv;
        COM_vy = ( m12 * (p1->momentum(1,i1)) + p2->momentum(1,i2) ) * gamma12_inv;
        COM_vz = ( m12 * (p1->momentum(2,i1)) + p2->momentum(2,i2) ) * gamma12_inv;
        COM_vsquare = COM_vx*COM_vx + COM_vy*COM_vy + COM_vz*COM_vz;
        COM_gamma = pow( 1.-COM_vsquare , -0.5);
        
        // Change the momentum to the COM frame (we work only on particle 1)
        // Quantities ending with "COM" are quantities of the particle expressed in the COM frame.
        term1 = (COM_gamma - 1.) / COM_vsquare;
        vcv1  = (COM_vx*(p1->momentum(0,i1)) + COM_vy*(p1->momentum(1,i1)) + COM_vz*(p1->momentum(2,i1)))/gamma1;
        vcv2  = (COM_vx*(p2->momentum(0,i2)) + COM_vy*(p2->momentum(1,i2)) + COM_vz*(p2->momentum(2,i2)))/gamma2;
        term2 = (term1*vcv1 - COM_gamma) * gamma1;
        px_COM = (p1->momentum(0,i1)) + term2*COM_vx;
        py_COM = (p1->momentum(1,i1)) + term2*COM_vy;
        pz_COM = (p1->momentum(2,i1)) + term2*COM_vz;
        p2_COM = px_COM*px_COM + py_COM*py_COM + pz_COM*pz_COM;
        p_COM  = sqrt(p2_COM);
        gamma1_COM = (1.-vcv1)*COM_gamma*gamma1;
        gamma2_COM = (1.-vcv2)*COM_gamma*gamma2;
        
        // Calculate some intermediate quantities
        term3 = COM_gamma * gamma12_inv;
        term4 = gamma1_COM * gamma2_COM;
        term5 = term4/p2_COM + m12;
        
        // Calculate coulomb log if necessary
        logL = coulomb_log;
        if( logL <= 0. ) { // if auto-calculation requested
            bmin = max( coeff1/m1/p_COM , abs(coeff2*qqm*term3*term5) ); // min impact parameter
            logL = 0.5*log(1.+patch->debye_length_squared/pow(bmin,2));
            if (logL < 2.) logL = 2.;
        }
        
        // Calculate the collision parameter s12 (similar to number of real collisions)
        s = coeff3 * logL * qqm2 * term3 * p_COM * term5*term5 / (gamma1*gamma2);
        
        // Low-temperature correction
        vrel = p_COM/term3/term4; // relative velocity
        smax = coeff4 * (m12+1.) * vrel / max(m12*n123,n223);
        if (s>smax) s = smax;
        
        // Pick the deflection angles according to Nanbu's theory
        cosX = cos_chi(s);
        sinX = sqrt( 1. - cosX*cosX );
        //!\todo make a faster rand by preallocating ??
        phi = twoPi * ((double)rand() / RAND_MAX);
        
        // Calculate combination of angles
        sinXcosPhi = sinX*cos(phi);
        sinXsinPhi = sinX*sin(phi);
        
        // Apply the deflection
        p_perp = sqrt( px_COM*px_COM + py_COM*py_COM );
        if( p_perp > 1.e-10*p_COM ) { // make sure p_perp is not too small
            inv_p_perp = 1./p_perp;
            newpx_COM = (px_COM * pz_COM * sinXcosPhi - py_COM * p_COM * sinXsinPhi) * inv_p_perp + px_COM * cosX;
            newpy_COM = (py_COM * pz_COM * sinXcosPhi + px_COM * p_COM * sinXsinPhi) * inv_p_perp + py_COM * cosX;
            newpz_COM = -p_perp * sinXcosPhi  +  pz_COM * cosX;
        } else { // if p_perp is too small, we use the limit px->0, py=0
            newpx_COM = p_COM * sinXcosPhi;
            newpy_COM = p_COM * sinXsinPhi;
            newpz_COM = p_COM * cosX;
        }
        
        // Random number to choose whether deflection actually applies.
        // This is to conserve energy in average when weights are not equal.
        //!\todo make a faster rand by preallocating ??
        U = ((double)rand() / RAND_MAX);
        
        // Go back to the lab frame and store the results in the particle array
        vcp = COM_vx * newpx_COM + COM_vy * newpy_COM + COM_vz * newpz_COM;
        if( U < W2/W1 ) { // deflect particle 1 only with some probability
            term6 = term1*vcp + gamma1_COM * COM_gamma;
            p1->momentum(0,i1) = newpx_COM + COM_vx * term6;
            p1->momentum(1,i1) = newpy_COM + COM_vy * term6;
            p1->momentum(2,i1) = newpz_COM + COM_vz * term6;
        }
        if( U < W1/W2 ) { // deflect particle 2 only with some probability
            term6 = -m12 * term1*vcp + gamma2_COM * COM_gamma;
            p2->momentum(0,i2) = -m12 * newpx_COM + COM_vx * term6;
            p2->momentum(1,i2) = -m12 * newpy_COM + COM_vy * term6;
            p2->momentum(2,i2) = -m12 * newpz_COM + COM_vz * term6;
        }
        
        // Handle ionization
        Ionization->apply(p1, i1, p2, i2);
        
        if( debug ) {
            smean    += s;
            logLmean += logL;
            //temperature += m1 * (sqrt(1.+pow(p1->momentum(0,i1),2)+pow(p1->momentum(1,i1),2)+pow(p1->momentum(2,i1),2))-1.);
        }
        
    } // end loop on pairs of particles
    
    // temporary to be removed
    Ionization->finish(patch->vecSpecies[(*sg1)[0]], patch->vecSpecies[(*sg2)[0]], params, patch);
    
    if(debug) {
        if( npairs>0 ) {
            ncol = (double)npairs;
            smean    /= ncol;
            logLmean /= ncol;
            //temperature /= ncol;
        }
    }
}


void Collisions::debug(Params& params, int itime, unsigned int icoll, VectorPatch& vecPatches)
{
    
    int debug_every = vecPatches(0)->vecCollisions[icoll]->debug_every;
    if( debug_every > 0 && itime % debug_every == 0 ) {
        
        unsigned int npatch = vecPatches.size();
        
        //vector<double> ncol(npatch, 0.);
        vector<double> smean(npatch, 0.);
        vector<double> logLmean(npatch, 0.);
        //vector<double>  temperature=(npatch, 0.);
        vector<double> debye_length_squared(npatch, 0.);
        
        // Collect info for all patches
        for( unsigned int ipatch=0; ipatch<npatch; ipatch++ ) {
            //ncol       [ipatch] = vecPatches(ipatch)->vecCollisions[icoll]->ncol       ;
            smean      [ipatch] = vecPatches(ipatch)->vecCollisions[icoll]->smean      ;
            logLmean   [ipatch] = vecPatches(ipatch)->vecCollisions[icoll]->logLmean   ;
            //temperature[ipatch] = vecPatches(ipatch)->vecCollisions[icoll]->temperature;
            debye_length_squared[ipatch] = vecPatches(ipatch)->debye_length_squared;
        }
        
        // Open the HDF5 file
        hid_t file_access = H5Pcreate(H5P_FILE_ACCESS);
        H5Pset_fapl_mpio(file_access, MPI_COMM_WORLD, MPI_INFO_NULL);
        hid_t fileId = H5Fopen(vecPatches(0)->vecCollisions[icoll]->filename.c_str(), H5F_ACC_RDWR, file_access);
        H5Pclose(file_access);
        // Create H5 group for the current timestep
        ostringstream name("");
        name << "t" << setfill('0') << setw(8) << itime;
        hid_t group = H5::group(fileId, name.str());
        // Define the size in memory for this MPI
        hsize_t mem_size[1] = {npatch};
        hid_t memspace  = H5Screate_simple(1, mem_size, NULL);
        // Define size and location in file
        hsize_t dimsf[1] = {(hsize_t)params.tot_number_of_patches};
        hid_t filespace = H5Screate_simple(1, dimsf, NULL);
        hsize_t offset[1] = {(hsize_t)vecPatches.refHindex_}, stride[1] = {1}, count[1] = {1}, block[1] = {npatch};
        H5Sselect_hyperslab(filespace, H5S_SELECT_SET, offset, stride, count, block);
        // Define transfer
        hid_t transfer = H5Pcreate(H5P_DATASET_XFER);
        H5Pset_dxpl_mpio(transfer, H5FD_MPIO_COLLECTIVE);
        // Define dataset property list
        hid_t plist_id = H5Pcreate(H5P_DATASET_CREATE);
        H5Pset_alloc_time(plist_id, H5D_ALLOC_TIME_EARLY );
        // Create new datasets for this timestep and write
        hid_t dset_id;
        dset_id  = H5Dcreate(group, "s", H5T_NATIVE_DOUBLE, filespace, H5P_DEFAULT, plist_id, H5P_DEFAULT);
        H5Dwrite( dset_id, H5T_NATIVE_DOUBLE, memspace, filespace, transfer, &smean[0] );
        H5Dclose(dset_id);
        dset_id  = H5Dcreate(group, "coulom_log", H5T_NATIVE_DOUBLE, filespace, H5P_DEFAULT, plist_id, H5P_DEFAULT);
        H5Dwrite( dset_id, H5T_NATIVE_DOUBLE, memspace, filespace, transfer, &logLmean[0] );
        H5Dclose(dset_id);
        //dset_id  = H5Dcreate(group, "temperature", H5T_NATIVE_DOUBLE, filespace, H5P_DEFAULT, plist_id, H5P_DEFAULT);
        //H5Dwrite( dset_id, H5T_NATIVE_DOUBLE, memspace, filespace, transfer, &temperature[0] );
        //H5Dclose(dset_id);
        // Close all
        H5Pclose(plist_id);
        H5Pclose( transfer );
        H5Sclose(filespace);
        H5Sclose(memspace);
        H5Gclose(group);
        H5Fclose(fileId);
    }

}


// Technique given by Nanbu in http://dx.doi.org/10.1103/PhysRevE.55.4642
//   to pick randomly the deflection angle cosine, in the center-of-mass frame.
// It involves the "s" parameter (~ collision frequency * deflection expectation)
//   and a random number "U".
// Technique slightly modified in http://dx.doi.org/10.1063/1.4742167
inline double Collisions::cos_chi(double s)
{
    
    double A, invA;
    //!\todo make a faster rand by preallocating ??
    double U = (double)rand() / RAND_MAX;
    
    if( s < 0.1 ) {
        if ( U<0.0001 ) U=0.0001; // ensures cos_chi > 0
        return 1. + s*log(U);
    }
    if( s < 3.  ) {
        // the polynomial has been modified from the article in order to have a better form
        invA = 0.00569578 +(0.95602 + (-0.508139 + (0.479139 + ( -0.12789 + 0.0238957*s )*s )*s )*s )*s;
        A = 1./invA;
        return  invA  * log( exp(-A) + 2.*U*sinh(A) );
    }
    if( s < 6.  ) {
        A = 3.*exp(-s);
        return (1./A) * log( exp(-A) + 2.*U*sinh(A) );
    }
    return 2.*U - 1.;
    
}




