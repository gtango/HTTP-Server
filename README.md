***
# C HTTP-Server
Work from Undergrad
***

##### Instructions for Building and Executing

1. Run 'make' to compile **httpserver.c** to create the **server** executable.
2. Run './server' followed by the port that you want to work on
    i) To run for you should open another terminal located in a different directory.
    ii) Utilize curl to test different HTTP requests:
        
    *  GET: curl http://localhost:(portNumber)/(fileNameWeWantOnServer)
    *  PUT: curl -T (fileOnClientSide) http://localhost:(portNumber)/(fileNameWeWantOnServer)
    *  HEAD: curl -I http://localhost:(portNumber)/(fileNameWeWantOnServer)

##### Notes

1. Does not work for mp4 files and such videos
