int main() {
    static int x;
    char a[++x];
    a[sizeof a - 1] = 0;
    int N;
    return a[0];
}