/*
Original author of the starter code
Tanzir Ahmed
Department of Computer Science & Engineering
Texas A&M University
Date: 2/8/20

Please include your Name, UIN, and the date below
Name: Ray Chu
UIN: 134003942
Date: 9/26/2025
*/
#include "common.h"
#include "FIFORequestChannel.h"

#include <sys/wait.h>
#include <iomanip>

using namespace std;

int main (int argc, char *argv[]) {
int opt;
int p = -1;
double t = -1.0;
int e = -1;
int m = MAX_MESSAGE; // default value for capacity

string filename = "";
while ((opt = getopt(argc, argv, "p:t:e:f:m:")) != -1) {
switch (opt) {
case 'p':
p = atoi (optarg);
break;
case 't':
t = atof (optarg);
break;
case 'e':
e = atoi (optarg);
break;
case 'f':
filename = optarg;
break;
// added
case 'm':
m = atoi(optarg); // use this in the below '-m'
break;
}
}

// 4.1
// give arguments for the server
// server needs './server', '-m', '<val for -m arg>', 'NULL'
// fork
// in the child, run execvp using the server arguments
pid_t pid = fork();
if (pid == -1) {
perror("fork failed");
exit(1);
}
if (pid == 0) {
int devnull = open("/dev/null", O_WRONLY);
if (devnull != -1) {
dup2(devnull, STDOUT_FILENO);
dup2(devnull, STDERR_FILENO);
close(devnull);
}

string mstr = to_string(m);
char* args[] = {(char*)"./server", (char*)"-m", (char*)(mstr.c_str()), nullptr};
execvp(args[0], args);
perror("child execvp failed");
_exit(1);
}

FIFORequestChannel chan("control", FIFORequestChannel::CLIENT_SIDE);

// Single datapoint, only run p,t,e != -1
if (p != -1 && t != -1 && e != -1) {
char buf[MAX_MESSAGE];
datamsg x(p, t, e);

memcpy(buf, &x, sizeof(datamsg));
chan.cwrite(buf, sizeof(datamsg));
double reply;
chan.cread(&reply, sizeof(double));
cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
}
// Else, if p != -1, request 1000 datapoints.
// Loop over 1st 1000 lines
// send request for ecg 1
// send request for ecg 2
// write line to received/x1.csv (match format)
else if (p != -1) {
ofstream out("x1.csv");
for (int i = 0; i < 1000; ++i) {
char buf[MAX_MESSAGE];
double ti = i * 0.004;

// Request ecg1
datamsg d1(p, ti, 1);
memcpy(buf, &d1, sizeof(datamsg));
chan.cwrite(buf, sizeof(datamsg));
double v1;
chan.cread(&v1, sizeof(double));

// Request ecg2
datamsg d2(p, ti, 2);
memcpy(buf, &d2, sizeof(datamsg));
chan.cwrite(buf, sizeof(datamsg));
double v2;
chan.cread(&v2, sizeof(double));

out << ti << "," << v1 << "," << v2 << "\n";
}
out.close();
system("mv x1.csv received/x1.csv");
}
// requesting
// one filemsg to get length
// multiple filemsg to get contents
// create new file with same name
// move to /received/
// (?) compare using diff
// test on 100mb file
else if (!filename.empty()) {

int header = sizeof(filemsg);
int fname_bytes = filename.size() + 1;
filemsg fm(0, 0);
int len = header + (fname_bytes);
char* buf = new char[len];
memcpy(buf, &fm, sizeof(filemsg));
strcpy(buf + header, filename.c_str());
chan.cwrite(buf, len);

size_t filelen = 0;
chan.cread(&filelen, sizeof(filelen));

vector<char> req(m);


string outpath = "received/" + filename;
ofstream out(outpath, ios::binary);

if (!out) {
    MESSAGE_TYPE qm = QUIT_MSG; chan.cwrite(&qm, sizeof(qm));
    int status = 0; waitpid(pid, &status, 0);
    return 1;
}

size_t max_payload = m - header - fname_bytes;
if (max_payload == 0) {
    MESSAGE_TYPE qm = QUIT_MSG; chan.cwrite(&qm, sizeof(qm));
    int status = 0; waitpid(pid, &status, 0);
    return 1;
}
vector<char> resp(max_payload);

size_t remaining = filelen;
size_t offset = 0;
while (remaining > 0) {
    size_t chunk = std::min(remaining, max_payload);

    filemsg fm(offset, chunk);
    memcpy(req.data(), &fm, header);
    strcpy(req.data() + header, filename.c_str());

    chan.cwrite(req.data(), header + fname_bytes);
    chan.cread(resp.data(), chunk);
    out.write(resp.data(), chunk);

    offset += chunk;
    remaining -= chunk;
}
out.close();

delete[] buf;
}

// closing the channel
MESSAGE_TYPE qm = QUIT_MSG;
chan.cwrite(&qm, sizeof(MESSAGE_TYPE));

if (p == -1 && t == -1 && e == -1) {
cout << "Client-side is done and exited\n";
}

// Reap child process
int status = 0;
waitpid(pid, &status, 0);

if (p == -1 && t == -1 && e == -1) {
cout << "Server terminated\n";
}

return 0;
}