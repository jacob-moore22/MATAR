/**********************************************************************************************
 � 2020. Triad National Security, LLC. All rights reserved.
 This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos
 National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S.
 Department of Energy/National Nuclear Security Administration. All rights in the program are
 reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear
 Security Administration. The Government is granted for itself and others acting on its behalf a
 nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare
 derivative works, distribute copies to the public, perform publicly and display publicly, and
 to permit others to do so.
 This program is open source under the BSD-3 License.
 Redistribution and use in source and binary forms, with or without modification, are permitted
 provided that the following conditions are met:
 1.  Redistributions of source code must retain the above copyright notice, this list of
 conditions and the following disclaimer.
 2.  Redistributions in binary form must reproduce the above copyright notice, this list of
 conditions and the following disclaimer in the documentation and/or other materials
 provided with the distribution.
 3.  Neither the name of the copyright holder nor the names of its contributors may be used
 to endorse or promote products derived from this software without specific prior
 written permission.
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **********************************************************************************************/
#include <stdio.h>
#include <array>
#include <vector>
#include <chrono>
#include <math.h>

#include "matar.h"

using namespace mtr; // matar namespace



// =================================================================
// Artificial Neural Network (ANN)
//
// For a single layer, we have x_i inputs with weights_{ij}, 
// creating y_j outputs.  We have
//     y_j = Fcn(b_j) = Fcn( Sum_i {x_i w_{ij}} )
// where the activation function Fcn is applied to b_j, creating 
// outputs y_j. For multiple layers, we have
//      b_j^l = Sum_i (x_i^{l-1} w_{ij}^l)
// where l is a layer, and as before, an activation function is  
// applied to b_j^l, creating outputs y_j^l.
// 
// =================================================================


// =================================================================
//
// Number of nodes in each layer including inputs and outputs
//
// =================================================================
std::vector <size_t> num_nodes_in_layer = {64000, 30000, 8000, 4000, 2000, 1000, 100} ;
//std::vector <size_t> num_nodes_in_layer = {50, 25} ;
// {9, 50, 100, 300, 200, 100, 20, 6}



// =================================================================
//
// data types and classes
//
// =================================================================

// array of ANN structs
struct ANNLayer_t{
    //input map will store every global id in the vector for simplificty of row-vector products in this example
    TpetraPartitionMap<long long int> output_partition_map; //map with all comms for row-vector product
    TpetraPartitionMap<long long int> output_unique_map; //submap of uniquely decomposed indices
    TpetraMVArray<real_t> distributed_outputs;
    TpetraCArray<real_t> distributed_weights;
    TpetraMVArray<real_t> distributed_biases; 

}; // end struct



// =================================================================
//
// functions
//
// =================================================================
void vec_mat_multiply(TpetraMVArray<real_t> &inputs,
                      TpetraMVArray<real_t> &outputs, 
                      TpetraCArray<real_t> &matrix){
    
    const size_t num_i = inputs.size();
    const size_t num_j = outputs.submap_size();

    using team_t = typename Kokkos::TeamPolicy<>::member_type;
    Kokkos::parallel_for ("MatVec", Kokkos::TeamPolicy<> (num_j, Kokkos::AUTO),
                 KOKKOS_LAMBDA (const team_t& team_h) {

        float sum = 0;
        int j = team_h.league_rank();
        Kokkos::parallel_reduce (Kokkos::TeamThreadRange (team_h, num_i),
                        [&] (int i, float& lsum) {
            lsum += inputs(i)*matrix(j,i);
        }, sum); // end parallel reduce
        int global_index = outputs.getSubMapGlobalIndex(j);
        int local_index = outputs.getMapLocalIndex(global_index);
        outputs(local_index) = sum;

    }); // end parallel for


    FOR_ALL(j,0,num_j, {
            int global_index = outputs.getSubMapGlobalIndex(j);
            int local_index = outputs.getMapLocalIndex(global_index);
            if(fabs(outputs(local_index) - num_i)>= 1e-15){
                printf("error in vec mat multiply test at row %d of %f\n", j, fabs(outputs(local_index) - num_i));
            }
    });
    
    return;

}; // end function

KOKKOS_INLINE_FUNCTION
float sigmoid(const float value){
    return 1.0/(1.0 + exp(-value));  // exp2f doesn't work with CUDA
}; // end function


KOKKOS_INLINE_FUNCTION
float sigmoid_derivative(const float value){
    float sigval = sigmoid(value);
    return sigval*(1.0 - sigval);  // exp2f doesn't work with CUDA
}; // end function




void forward_propagate_layer(TpetraMVArray<real_t> &inputs,
                             TpetraMVArray<real_t> &outputs, 
                             TpetraCArray<real_t> &weights,
                             const TpetraMVArray<real_t> &biases){
    
    const size_t num_i = inputs.size();
    const size_t num_j = outputs.submap_size();

    //perform comms to get full input vector for row vector products on matrix
    //VERY SIMPLE EXAMPLE OF COMMS; THIS IS A TERRIBLE WAY TO DECOMPOSE THE PROBLEM

/*    
    FOR_ALL(j, 0, num_j,{

    	//printf("thread = %d \n", omp_get_thread_num());

            float value = 0.0;
            for(int i=0; i<num_i; i++){
                // b_j = Sum_i {x_i w_{ij}}
                value += inputs(i)*weights(i,j);
            } // end for

            // apply activation function, sigmoid on a float, y_j = Fcn(b_j)
            outputs(j) = sigmoid(value);

        }); // end parallel for
*/     


    // For a GPU, use the nested parallelism below here
    using team_t = typename Kokkos::TeamPolicy<>::member_type;
    Kokkos::parallel_for ("MatVec", Kokkos::TeamPolicy<> (num_j, Kokkos::AUTO),
                 KOKKOS_LAMBDA (const team_t& team_h) {

        float sum = 0;
        int j = team_h.league_rank();
        Kokkos::parallel_reduce (Kokkos::TeamThreadRange (team_h, num_i),
                        [&] (int i, float& lsum) {
            lsum += inputs(i)*weights(j,i) + biases(j);
        }, sum); // end parallel reduce
        int global_index = outputs.getSubMapGlobalIndex(j);
        int local_index = outputs.getMapLocalIndex(global_index);
        outputs(local_index) = 1.0/(1.0 + exp(-sum)); 

    }); // end parallel for
    


    return;

}; // end function


void set_biases(const TpetraMVArray<real_t> &biases){
    const size_t num_j = biases.size();

    FOR_ALL(j,0,num_j, {
		    biases(j) = 0.0;
	}); // end parallel for

}; // end function


void set_weights(const TpetraCArray<real_t> &weights){

    const size_t num_i = weights.dims(0);
    const size_t num_j = weights.dims(1);
    
	FOR_ALL(i,0,num_i,
	        j,0,num_j, {
		    
		    weights(i,j) = 1.0;
	}); // end parallel for

}; // end function


// =================================================================
//
// Main function
//
// =================================================================
int main(int argc, char* argv[])
{   
    MPI_Init(&argc, &argv);
    int process_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &process_rank);
    Kokkos::initialize(argc, argv);
    {

        // =================================================================
        // allocate arrays
        // =================================================================

        // note: the num_nodes_in_layer has the inputs into the ANN, so subtract 1 for the layers
        size_t num_layers = num_nodes_in_layer.size()-1;  

        CArray <ANNLayer_t> ANNLayers(num_layers); // starts at 1 and goes to num_layers

        // input and ouput values to ANN
        TpetraPartitionMap<long long int> input_pmap, input_unique_pmap;
        DCArrayKokkos<long long int> all_layer_indices(num_nodes_in_layer[0]);
        FOR_ALL(i,0,num_nodes_in_layer[0], {
            all_layer_indices(i) = i;
        });
        all_layer_indices.update_host();  // copy inputs to device
        //map of all indices in this layer to be used for row-vector product (in practice, this would not include all indices in the layer)
        input_pmap = TpetraPartitionMap<long long int>(all_layer_indices);

        //map that decomposes indices of this onto set of processes uniquely (used to demonstrate comms for above)
        input_unique_pmap = TpetraPartitionMap<long long int>(num_nodes_in_layer[0]);
        TpetraMVArray<real_t> inputs(input_pmap); //rows decomposed onto processes
        //comming from subview requires both the original map and the submap to be composed of contiguous indices
        inputs.own_comm_setup(input_unique_pmap); //tells the vector its communicating from a contiguous subset of its own data

        // set the strides
        // layer 0 are the inputs to the ANN
        // layer n-1 are the outputs from the ANN
        for (size_t layer=0; layer<num_layers; layer++){

            // dimensions
            size_t num_i = num_nodes_in_layer[layer];
            size_t num_j = num_nodes_in_layer[layer+1];
            DCArrayKokkos<long long int> all_current_layer_indices(num_nodes_in_layer[layer+1]);
            FOR_ALL(i,0,num_nodes_in_layer[layer+1], {
                all_current_layer_indices(i) = i;
            });

            ANNLayers(layer).output_partition_map = TpetraPartitionMap<long long int>(all_current_layer_indices);
            ANNLayers(layer).output_unique_map = TpetraPartitionMap<long long int>(num_nodes_in_layer[layer+1]); 
            ANNLayers(layer).distributed_outputs = TpetraMVArray<real_t> (ANNLayers(layer).output_partition_map);
            //comming from subview requires both the original map and the submap to be composed of contiguous indices
            ANNLayers(layer).distributed_outputs.own_comm_setup(ANNLayers(layer).output_unique_map);
            // allocate the weights in this layer
            ANNLayers(layer).distributed_weights = TpetraCArray<real_t> (num_j, num_i);
            ANNLayers(layer).distributed_biases = TpetraMVArray<real_t> (num_j);

        } // end for


        // =================================================================
        // set weights, biases, and inputs
        // =================================================================
        
        // inputs to ANN
        size_t local_input_size = inputs.submap_size();
        //std::cout << "full_input_size " << input_pmap.num_global_ << "\n";
        for (size_t i=0; i<local_input_size; i++) {
            int global_index = inputs.getSubMapGlobalIndex(i);
            int local_index = inputs.getMapLocalIndex(global_index);
            inputs.host(local_index) = 1.0;
        }
        
        // //debug print
        // std::ostream &out = std::cout;
        // Teuchos::RCP<Teuchos::FancyOStream> fos;
        // fos = Teuchos::fancyOStream(Teuchos::rcpFromRef(out));
        // inputs.tpetra_sub_vector->describe(*fos,Teuchos::VERB_EXTREME);
        
        inputs.update_device();  // copy inputs to device
        inputs.perform_comms(); //distribute to full map for row-vector product

        // for (size_t i=0; i<num_nodes_in_layer[0]; i++) {
        //     std::cout << "input at " << i << " is " << inputs(i) << "\n";
        // }

        // weights of the ANN
        for (size_t layer=0; layer<num_layers; layer++){

            set_weights(ANNLayers(layer).distributed_weights);
            set_biases(ANNLayers(layer).distributed_biases);

        } // end for over layers



        // =================================================================
        // Testing vec matrix multiply
        // =================================================================        
        vec_mat_multiply(inputs,
                         ANNLayers(0).distributed_outputs,
                         ANNLayers(0).distributed_weights); 
        
        if(process_rank==0)
            std::cout << "vec mat multiply test completed \n";




        // =================================================================
        // Use the ANN
        // =================================================================
        
        MPI_Barrier(MPI_COMM_WORLD);
        auto time_1 = std::chrono::high_resolution_clock::now();

        // forward propogate

        // layer 1, hidden layer 0, uses the inputs as the input values
        forward_propagate_layer(inputs,
                                ANNLayers(0).distributed_outputs,
                                ANNLayers(0).distributed_weights,
                                ANNLayers(0).distributed_biases); 

        // layer 2 through n-1, layer n-1 goes to the output
        for (size_t layer=1; layer<num_layers; layer++){
            
            ANNLayers(layer-1).distributed_outputs.perform_comms(); //distribute to full map for row-vector product
            // go through this layer, the fcn takes(inputs, outputs, weights)
            forward_propagate_layer(ANNLayers(layer-1).distributed_outputs, 
                                    ANNLayers(layer).distributed_outputs,
                                    ANNLayers(layer).distributed_weights,
                                    ANNLayers(layer).distributed_biases);
            
        } // end for
        
        MPI_Barrier(MPI_COMM_WORLD);
        auto time_2 = std::chrono::high_resolution_clock::now();

        std::chrono::duration <float, std::milli> ms = time_2 - time_1;
        if(process_rank==0)
            std::cout << "runtime of ANN test = " << ms.count() << "ms\n\n";
        
        
        // =================================================================
        // Copy values to host
        // =================================================================
        ANNLayers(num_layers-1).distributed_outputs.update_host();

        if(process_rank==0)
            std::cout << "output values grid: \n";
        std::flush(std::cout);
        MPI_Barrier(MPI_COMM_WORLD);
        std::stringstream output_stream;
        size_t local_output_size = ANNLayers(num_layers-1).distributed_outputs.submap_size();
        for (size_t val=0; val < local_output_size; val++){
            int global_index = ANNLayers(num_layers-1).distributed_outputs.getSubMapGlobalIndex(val);
            int local_index = ANNLayers(num_layers-1).distributed_outputs.getMapLocalIndex(global_index);
            output_stream << " " << ANNLayers(num_layers-1).distributed_outputs.host(local_index);
            if(val%10==0) output_stream << std::endl;
        } // end for
        std::cout << output_stream.str();
        std::flush(std::cout);

        //test repartition; assume a 10 by 10 grid of outputs from ANN
        //assign coords to each grid point, find a partition of the grid, then repartition output layer using new map
        TpetraMVArray<real_t> output_grid(100, 2); //array of 2D coordinates for 10 by 10 grid of points
        
        //populate coords
        FOR_ALL(i,0,output_grid.dims(0), {
		    output_grid(i, 0) = i/10;
            output_grid(i, 1) = i%10;
	    }); // end parallel for

        MPI_Barrier(MPI_COMM_WORLD);
        if(process_rank==0){ 
            std::cout << std::endl;
            std::cout << " Map before repartitioning" << std::endl;
        }
        std::flush(std::cout);
        output_grid.pmap.print();
        
        MPI_Barrier(MPI_COMM_WORLD);
        output_grid.repartition_vector();
        if(process_rank==0){ 
            std::cout << std::endl;
            std::cout << " Map after repartitioning" << std::endl;
        }
        output_grid.pmap.print();

        if(process_rank==0){ 
            std::cout << std::endl;
            std::cout << " Grid components per rank after repartitioning" << std::endl;
        }
        output_grid.print();

        //get repartitioned map to distribute new arrays with it
        TpetraPartitionMap<long long int> partitioned_output_map = output_grid.pmap;
        TpetraMVArray<real_t> partitioned_output_values(partitioned_output_map, "partitioned output values");

        //construct a unique source vector from ANN output using the subview constructor
        //(for example's sake this is in fact a copy of the subview wrapped by the output as well)
        TpetraMVArray<real_t> sub_output_values(ANNLayers(num_layers-1).distributed_outputs, ANNLayers(num_layers-1).distributed_outputs.comm_pmap,
                                                 ANNLayers(num_layers-1).distributed_outputs.comm_pmap.getMinGlobalIndex());

        //general communication object between two vectors/arrays
        TpetraCommunicationPlan<real_t> output_comms(partitioned_output_values, sub_output_values);
        output_comms.execute_comms();
        partitioned_output_values.print();

    } // end of kokkos scope

    Kokkos::finalize();
    MPI_Barrier(MPI_COMM_WORLD);
    if(process_rank==0)
        printf("\nfinished\n\n");
    MPI_Finalize();


    return 0;
}


