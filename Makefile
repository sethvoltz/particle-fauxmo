all: firmware.bin

firmware.bin:
	particle compile photon ./ --saveTo firmware.bin

clean:
	rm -f firmware.bin
