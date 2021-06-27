CC = nvcc
#CFLAGS = -O2 -g -arch sm_60
CFLAGS = -O2 -g 
#LDFLAGS = -arch sm_60
LDFLAGS = 
LIBS = 

all: diffusion

mm: diffusion.o
	$(CC) $^ $(LIBS) -o $@ $(LDFLAGS)

%.o : %.cu
	$(CC) $(CFLAGS) -c $< -o $*.o

clean:
	rm -f *.o
	rm -f *~
	rm -f diffusion
