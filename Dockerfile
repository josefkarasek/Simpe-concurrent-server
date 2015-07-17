FROM fedora:22
MAINTAINER "Josef Karasek <jkarasek@redhat.com>"
EXPOSE 9999
COPY ["/server_dir/server", "/usr/bin/server"] 
COPY ["pepa.png", "/usr/bin/pepa.png"]
COPY ["pepa.png", "/pepa.png"]
CMD ["/usr/bin/server", "-p", "9999", "-d", "10000"]

