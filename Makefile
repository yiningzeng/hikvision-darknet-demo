Demo: ImageProcess.cpp
	g++ -g  ImageProcess.cpp processor.cpp `pkg-config --cflags --libs opencv` -I../../../include -Wl,-rpath=../../../lib/64 -L../../../lib/64 -lMvCameraControl -I ./include -L ./include -ldark -o ImageProcess
	

clean:
	rm ImageProcess -rf
