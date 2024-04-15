FROM gcc:latest

WORKDIR /home/app
VOLUME ["/home/app"]

RUN apt-get update; apt-get install -y gdb
CMD [ "bash" ]