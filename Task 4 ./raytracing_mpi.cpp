#include "minirt/minirt.h"

#include <omp.h>
#include <cmath>
#include <mpi.h>
#include <iostream>

using namespace minirt;
using namespace std;

void initScene(Scene& scene) {
    Color red{ 1, 0.2, 0.2 };
    Color blue{ 0.2, 0.2, 1 };
    Color green{ 0.2, 1, 0.2 };
    Color white{ 0.8, 0.8, 0.8 };
    Color yellow{ 1, 1, 0.2 };

    Material metallicRed{ red, white, 50 };
    Material mirrorBlack{ Color {0.0}, Color {0.9}, 1000 };
    Material matteWhite{ Color {0.7}, Color {0.3}, 1 };
    Material metallicYellow{ yellow, white, 250 };
    Material greenishGreen{ green, 0.5, 0.5 };

    Material transparentGreen{ green, 0.8, 0.2 };
    transparentGreen.makeTransparent(1.0, 1.03);
    Material transparentBlue{ blue, 0.4, 0.6 };
    transparentBlue.makeTransparent(0.9, 0.7);

    scene.addSphere(Sphere{ {0, -2, 7}, 1, transparentBlue });
    scene.addSphere(Sphere{ {-3, 2, 11}, 2, metallicRed });
    scene.addSphere(Sphere{ {0, 2, 8}, 1, mirrorBlack });
    scene.addSphere(Sphere{ {1.5, -0.5, 7}, 1, transparentGreen });
    scene.addSphere(Sphere{ {-2, -1, 6}, 0.7, metallicYellow });
    scene.addSphere(Sphere{ {2.2, 0.5, 9}, 1.2, matteWhite });
    scene.addSphere(Sphere{ {4, -1, 10}, 0.7, metallicRed });

    scene.addLight(PointLight{ {-15, 0, -15}, white });
    scene.addLight(PointLight{ {1, 1, 0}, blue });
    scene.addLight(PointLight{ {0, -10, 6}, red });

    scene.setBackground({ 0.05, 0.05, 0.08 });
    scene.setAmbient({ 0.1, 0.1, 0.1 });
    scene.setRecursionLimit(20);

    scene.setCamera(Camera{ {0, 0, -20}, {0, 0, 0} });
}

int main(int argc, char **argv) {
    
    MPI_Init(&argc, &argv);
    
    int viewPlaneResolutionX = (argc > 1 ? stoi(argv[1]) : 600);
    int viewPlaneResolutionY = (argc > 2 ? stoi(argv[2]) : 600);
    int numOfSamples = (argc > 3 ? stoi(argv[3]) : 1);
    string sceneFile = (argc > 4 ? argv[4] : "");

    Scene scene;
    if (sceneFile.empty()) {
        initScene(scene);
    } else {
        scene.loadFromFile(sceneFile);
    }

    const double backgroundSizeX = 4;
    const double backgroundSizeY = 4;
    const double backgroundDistance = 15;

    const double viewPlaneDistance = 5;
    const double viewPlaneSizeX = backgroundSizeX * viewPlaneDistance / backgroundDistance;
    const double viewPlaneSizeY = backgroundSizeY * viewPlaneDistance / backgroundDistance;

    ViewPlane viewPlane {viewPlaneResolutionX, viewPlaneResolutionY,
                         viewPlaneSizeX, viewPlaneSizeY, viewPlaneDistance};
                         
    int rank, size;
    
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    // TODO: partition the image onto pieces (blocks).
    // Each process should compute a different piece of the image.
    // You should correctly compute the size of the piece for each process.
    // Generally, your program should work correctly for any image size and number of processes. 
    int mySizeX = viewPlaneResolutionX;
    int mySizeY = viewPlaneResolutionY;

    int block_size = mySizeY/size;
    int block_start = rank * block_size;
    int last_rank = size-1;
    if (rank==last_rank){block_size = block_size + (mySizeY%size);}
    // Each process stores its piece only.
    Image myPiece(mySizeX, block_size);
    
    double ts = MPI_Wtime();
    // TODO: make each process to use correct pixel indices in computePixel for its piece.
    // Hint: for that you need to know where its piece is located in the image (an offset from the (0,0) global index).
    for(int x = 0; x < mySizeX; x++)
    for(int y = 0; y < block_size; y++) {
        const auto color = viewPlane.computePixel(scene, x, y + block_start, numOfSamples);
        myPiece.set(x, y, color);
    }
    double te = MPI_Wtime();
    
    double time = te - ts;
    
    // TODO: compute the maximum time from all processes using MPI_Reduce and output it only once.
    cout << rank << " Time = " << time << endl;
    

    double max_time;

    MPI_Reduce(&time, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
	cout << "Max time = " << max_time << endl;
    }

    // We are sending computed piece to the main (0) process as array of doubles.
    MPI_Request send_request;
    MPI_Isend(myPiece.getData(), mySizeX * block_size * 3, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, &send_request);

    // The main process will gather the final (resulting) image.
    if (rank == 0) {
        // This will be the final image.
        Image finalImage(viewPlaneResolutionX, viewPlaneResolutionY);
        
        for (int src_rank = 0; src_rank < size; src_rank++) {
            // TODO: in general case src_rank's piece size can be different from our (rank 0) piece size.
            // Think how to handle this correctly.
            int srcSizeX = mySizeX;
	    int src_block_size = mySizeY/size;
            int srcSizeY = mySizeY/size; 
	    if (src_rank == last_rank){srcSizeY = srcSizeY + (mySizeY%size);}
            // Allocating temporary space to receive an image piece from src_rank process.
            Image srcPiece(srcSizeX, srcSizeY);
            // Receiving a piece.
            MPI_Recv(srcPiece.getData(), srcSizeX * srcSizeY * 3, MPI_DOUBLE, src_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            // TODO: copy the received piece into the correct place in the final image.
            // Hint: by knowing a rank of sending process (src_rank) and how pieces are distributed, 
            // you can compute the correct location for its piece.
	    for(int x = 0; x < mySizeX; x++)
    	    for(int y = 0; y < srcSizeY; y++) {
        	finalImage.set(x, y+(src_rank*src_block_size), srcPiece.get(x, y));
    	    }

        }
        finalImage.saveJPEG("raytracing_" + to_string(size) + ".jpg");
    }
       

    // Finishing send operation.
    MPI_Wait(&send_request, MPI_STATUS_IGNORE);

    MPI_Finalize();

    return 0;
}