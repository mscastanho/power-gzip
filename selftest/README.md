# Test the NX GZIP accelerator

Note that the code here is intended for testing the nx-gzip hardware function.
They are not intended for demonstrating performance or compression ratio.
By being simplistic, these selftests expect to allocate the entire set of source
and target pages in the memory so it needs enough memory to work.
For more information and source code consider using:
https://github.com/libnxz/power-gzip

## Requirements

Verify that following device exists:
  /dev/crypto/nx-gzip
If you get a permission error run as sudo or set the device permissions:
   sudo chmod go+rw /dev/crypto/nx-gzip
However, chmod may not survive across boots. You may create a udev file such
as described in:
https://github.com/libnxz/power-gzip/wiki/Enable-nx-gzip-on-POWER9#nx-gzip-device-permissions

# Manual tests

To manually build and run:
$ gcc -O3 -I./include -o gzfht_test gzfht_test.c gzip_vas.c
$ gcc -O3 -I./include -o gunz_test gunz_test.c gzip_vas.c


Compress any file using Fixed Huffman mode. Output will have a .nx.gz suffix:
$ ./gzfht_test gzip_vas.c
file gzip_vas.c read, 6413 bytes
compressed 6413 to 3124 bytes total, crc32 checksum = abd15e8a


Uncompress the previous output. Output will have a .nx.gunzip suffix:
./gunz_test gzip_vas.c.nx.gz
gzHeader FLG 0
00 00 00 00 04 03
gzHeader MTIME, XFL, OS ignored
computed checksum abd15e8a isize 0000190d
stored   checksum abd15e8a isize 0000190d
decomp is complete: fclose


Compare two files:
$ sha1sum gzip_vas.c.nx.gz.nx.gunzip gzip_vas.c
bf43e3c0c3651f5f22b6f9784cd9b1eeab4120b6  gzip_vas.c.nx.gz.nx.gunzip
bf43e3c0c3651f5f22b6f9784cd9b1eeab4120b6  gzip_vas.c
