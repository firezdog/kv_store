from io import TextIOWrapper
from os.path import exists
from fcntl import flock, LOCK_EX, LOCK_UN

class Database:
    _idx_fd: TextIOWrapper
    _dat_fd: TextIOWrapper
    _nhash: int
    _PTR_SIZE = 7   # the size (in chars) of a file offset pointer

    def __init__(self, name: str, nhash=256, overwrite=False):
        self._nhash = nhash

        idx_file = f'{name}.idx'
        dat_file = f'{name}.dat'

        # !! the overwrite flag is dangerous and exists only for testing
        if exists(dat_file) and exists(idx_file) and not overwrite:
            # open without truncating
            self._idx_fd = open(idx_file, 'a+')
            self._dat_fd = open(dat_file, 'a+')
        else:
            # create new file and initialize hash line / free list
            self._idx_fd = open(idx_file, 'w')
            self._dat_fd = open(dat_file, 'w')

            # lock while we initialize the first line of the index file -- exclusive as no operations can proceed before init
            flock(self._idx_fd, LOCK_EX)
            # write 1 + nhash hash entries (one more for the free list) and newline
            self._idx_fd.write(''.join([f'{0:>{f"{self._PTR_SIZE}"}}' for i in range(self._nhash + 1)]))
            self._idx_fd.write('\n')
            # unlock
            flock(self._idx_fd, LOCK_UN)


    def __del__(self):
        self._idx_fd.close()
        self._dat_fd.close()


if __name__ == '__main__':
    db = Database('example', overwrite=True)