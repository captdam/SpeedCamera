gcc tri.c -lm -O3 -o tri
gcc points.c -lm -O3 -o points
./points 1280 720 < on3.txt > on3-720.map
./tri 1280 720 < 403s.txt > 403s-720.map
sha1sum on3-720.map
sha1sum 403s-720.map