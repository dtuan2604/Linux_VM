CC         = gcc
CFLAGS     = -Wall -g

STANDARD   = oss.h config.h 
SRC        = queue.h linkedlist.h helper.h 

OBJ        = queue.o linkedlist.o helper.o 
MASTER_OBJ = oss.o
USER_OBJ   = user.o

MASTER     = oss
USER       = user

OUTPUT    = $(MASTER) $(USER)
all: $(OUTPUT)


%.o: %.c $(STANDARD) $(SRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(MASTER): $(MASTER_OBJ) $(OBJ)
	$(CC) $(CFLAGS) $(MASTER_OBJ) $(OBJ) -o $(MASTER)

$(USER): $(USER_OBJ)
	$(CC) $(CFLAGS) $(USER_OBJ) -o $(USER)

clean:
	rm $(OUTPUT) *.o *.txt

