ntsc : synthtables MakeCbTables
pal : synthtablesPAL MakeCbTablesPAL

all : ntsc

synthtables : synthtables.c
	gcc -o $@ $^ -g -lm
	./synthtables

MakeCbTables : MakeCbTables.c
	gcc -o $@ $^ -g -lm
	./MakeCbTables

synthtablesPAL : synthtables.c
	gcc -DPAL -o $@ $^ -g -lm
	./synthtablesPAL

MakeCbTablesPAL : MakeCbTables.c
	gcc -DPAL -o $@ $^ -g -lm
	./MakeCbTablesPAL

clean :
	rm -rf synthtables synthtablesPAL MakeCbTables MakeCbTablesPAL CbTable.h CbTable.c broadcast_tables.c broadcast_tables.h
