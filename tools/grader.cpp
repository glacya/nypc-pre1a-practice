// 채점 및 검증기.
// inputs/ 의 모든 케이스에 대해 솔루션을 실행(stdin 입력, stdout 출력)하고,
// 출력을 검증/채점하여 results/result_{yymmdd}_{HHMMSS}.json 에 기록한다
// (같은 이름이 이미 있으면 result_{yymmdd}_{HHMMSS}_{num}.json, num = 1, 2, ...).
// 각 케이스의 출력은 outputs/{num}.txt 로 저장한다(최신 실행 결과로 덮어씀).
//
// 사용법: grader <solution_cmd> [--inputs DIR] [--results DIR] [--outputs DIR]
//   예) build/grader build/solution
#include <bits/stdc++.h>
#include <filesystem>
using namespace std;
namespace fs = std::filesystem;

// 문제의 채점 표: 케이스 번호 -> {Cost', S}
const map<int, pair<long long, long long>> TABLE = {
    {1, {1, 50000}},    {2, {4, 50000}},    {3, {3, 50000}},    {4, {9, 50000}},
    {5, {18, 50000}},   {6, {12, 50000}},   {7, {37, 50000}},   {8, {129, 50000}},
    {9, {151, 50000}},  {10, {127, 50000}}, {11, {80, 100000}}, {12, {37, 100000}},
    {13, {35, 100000}}, {14, {238, 100000}}, {15, {72, 100000}},
};

struct CaseResult {
    int id = 0;
    int n = 0, m = 0, c = 0;
    bool valid = false;
    string error;
    long long length = 0, remaining = 0, cost = 0;
    long long costRef = 0, maxScore = 0, score = 0;
    double timeMs = 0;
};

string jsonEscape(const string& s) {
    string r;
    for (char ch : s) {
        if (ch == '"' || ch == '\\') { r += '\\'; r += ch; }
        else if (ch == '\n') r += "\\n";
        else if ((unsigned char)ch < 0x20) r += ' ';
        else r += ch;
    }
    return r;
}

string shellQuote(const string& s) {
    string r = "'";
    for (char ch : s) {
        if (ch == '\'') r += "'\\''";
        else r += ch;
    }
    return r + "'";
}

// 입력과 출력을 받아 검증. 실패 시 error를 채우고 false.
bool simulate(const string& input, const string& rawOut, CaseResult& res) {
    istringstream in(input);
    int N, M, C;
    in >> N >> M >> C;
    int K[2] = {0, 0};
    for (int i = 0; i < C; i++) in >> K[i];
    vector<string> g(N);
    int pr[2] = {0, 0}, pc[2] = {0, 0};
    long long blocks = 0;
    for (int i = 0; i < N; i++) {
        in >> g[i];
        for (int j = 0; j < M; j++) {
            if (g[i][j] == 'B') { pr[0] = i; pc[0] = j; g[i][j] = '.'; }
            else if (g[i][j] == 'D') { pr[1] = i; pc[1] = j; g[i][j] = '.'; }
            else if (g[i][j] == '@') blocks++;
        }
    }
    res.n = N; res.m = M; res.c = C;

    // 앞뒤 공백만 허용
    size_t b = rawOut.find_first_not_of(" \t\r\n");
    size_t e = rawOut.find_last_not_of(" \t\r\n");
    string out = (b == string::npos) ? "" : rawOut.substr(b, e - b + 1);
    res.length = out.size();
    // 빈 출력은 "아무 행동도 하지 않음"으로 허용한다 (블럭이 남으므로 cost = 100000 + 블럭 수).
    if (out.size() > 100000) { res.error = "output longer than 100000"; return false; }

    auto inb = [&](int r, int c) { return r >= 0 && r < N && c >= 0 && c < M; };
    for (size_t t = 0; t < out.size(); t++) {
        int p = (int)(t % C);
        char ch = out[t];
        int dr = 0, dc = 0;
        switch (ch) {
            case 'U': dr = -1; break;
            case 'D': dr = 1; break;
            case 'L': dc = -1; break;
            case 'R': dc = 1; break;
            case 'B': break;
            default:
                res.error = "invalid character at " + to_string(t);
                return false;
        }
        if (ch == 'B') {
            for (int d = 0; d < 4; d++) {
                int ddr = (d == 0) ? -1 : (d == 1) ? 1 : 0;
                int ddc = (d == 2) ? -1 : (d == 3) ? 1 : 0;
                for (int s = 1; s <= K[p]; s++) {
                    int nr = pr[p] + ddr * s, nc = pc[p] + ddc * s;
                    if (!inb(nr, nc) || g[nr][nc] == '#') break;
                    if (g[nr][nc] == '@') { g[nr][nc] = '.'; blocks--; break; }
                }
            }
        } else {
            int nr = pr[p] + dr, nc = pc[p] + dc;
            if (!inb(nr, nc)) { res.error = "move out of grid at " + to_string(t); return false; }
            if (g[nr][nc] == '#' || g[nr][nc] == '@') {
                res.error = "move into obstacle/block at " + to_string(t);
                return false;
            }
            pr[p] = nr; pc[p] = nc;
        }
    }
    res.remaining = blocks;
    res.cost = blocks == 0 ? (long long)out.size() : 100000 + blocks;
    return true;
}

string runSolution(const string& cmd, const string& inputPath, double& ms) {
    string full = cmd + " < " + shellQuote(inputPath);
    auto st = chrono::steady_clock::now();
    FILE* f = popen(full.c_str(), "r");
    string out;
    if (f) {
        char buf[65536];
        size_t k;
        while ((k = fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, k);
        pclose(f);
    }
    ms = chrono::duration<double, milli>(chrono::steady_clock::now() - st).count();
    return out;
}

int main(int argc, char** argv) {
    string solution, inputsDir = "inputs", resultsDir = "results", outputsDir = "outputs";
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        if (a == "--inputs" && i + 1 < argc) inputsDir = argv[++i];
        else if (a == "--results" && i + 1 < argc) resultsDir = argv[++i];
        else if (a == "--outputs" && i + 1 < argc) outputsDir = argv[++i];
        else if (solution.empty()) solution = a;
        else { cerr << "unknown argument: " << a << '\n'; return 1; }
    }
    if (solution.empty()) {
        cerr << "usage: grader <solution_cmd> [--inputs DIR] [--results DIR] [--outputs DIR]\n";
        return 1;
    }

    vector<fs::path> files;
    for (auto& ent : fs::directory_iterator(inputsDir))
        if (ent.is_regular_file() && ent.path().extension() == ".txt") files.push_back(ent.path());
    sort(files.begin(), files.end());

    fs::create_directories(outputsDir);
    vector<CaseResult> results;
    long long total = 0, maxTotal = 0;
    printf("%-4s %-9s %-7s %8s %8s %8s %10s %9s  %s\n", "case", "NxM", "C", "cost", "ref",
           "remain", "score", "time(ms)", "note");
    for (auto& p : files) {
        CaseResult r;
        r.id = atoi(p.stem().string().c_str());
        auto it = TABLE.find(r.id);
        if (it != TABLE.end()) { r.costRef = it->second.first; r.maxScore = it->second.second; }

        ifstream fin(p);
        string input((istreambuf_iterator<char>(fin)), istreambuf_iterator<char>());
        string out = runSolution(solution, p.string(), r.timeMs);
        r.valid = simulate(input, out, r);
        // 출력은 유효 여부와 관계없이 그대로 저장한다 (공유/디버깅용).
        ofstream(fs::path(outputsDir) / p.filename()) << out;
        if (r.valid && r.cost == 0)
            r.score = r.maxScore;  // 블럭이 없는 입력에 빈 출력: 비용 0
        else if (r.valid)
            r.score = (long long)floor((double)r.maxScore * min((double)r.costRef / r.cost, 1.0));
        total += r.score;
        maxTotal += r.maxScore;

        string nm = to_string(r.n) + "x" + to_string(r.m);
        printf("%-4d %-9s %-7d %8lld %8lld %8lld %10lld %9.1f  %s\n", r.id, nm.c_str(), r.c,
               r.valid ? r.cost : -1, r.costRef, r.remaining, r.score, r.timeMs,
               r.valid ? "" : r.error.c_str());
        results.push_back(r);
    }
    printf("total: %lld / %lld\n", total, maxTotal);

    // JSON 실행 기록 작성 (파일 하나 = 실행 하나)
    time_t now = time(nullptr);
    tm lt = *localtime(&now);
    char stampBuf[16], tsBuf[32];
    strftime(stampBuf, sizeof stampBuf, "%y%m%d_%H%M%S", &lt);
    strftime(tsBuf, sizeof tsBuf, "%Y-%m-%dT%H:%M:%S%z", &lt);

    ostringstream js;
    js << "{\n";
    js << "  \"graded_at\": \"" << tsBuf << "\",\n";
    js << "  \"solution\": \"" << jsonEscape(solution) << "\",\n";
    js << "  \"summary\": \"\",\n";
    js << "  \"total_score\": " << total << ",\n";
    js << "  \"max_score\": " << maxTotal << ",\n";
    js << "  \"cases\": [\n";
    for (size_t i = 0; i < results.size(); i++) {
        auto& r = results[i];
        js << "    {\"case\": " << r.id << ", \"n\": " << r.n << ", \"m\": " << r.m
           << ", \"c\": " << r.c << ", \"valid\": " << (r.valid ? "true" : "false")
           << ", \"error\": " << (r.valid ? "null" : "\"" + jsonEscape(r.error) + "\"")
           << ", \"length\": " << r.length
           << ", \"remaining_blocks\": " << (r.valid ? to_string(r.remaining) : "null")
           << ", \"cost\": " << (r.valid ? to_string(r.cost) : "null")
           << ", \"cost_ref\": " << r.costRef << ", \"score\": " << r.score
           << ", \"max_score\": " << r.maxScore << ", \"time_ms\": " << fixed
           << setprecision(1) << r.timeMs << "}" << (i + 1 < results.size() ? "," : "") << "\n";
    }
    js << "  ]\n";
    js << "}\n";

    fs::create_directories(resultsDir);
    // 기존 기록을 덮어쓰지 않도록, 이름이 겹치면 _1, _2, ... 를 붙인다.
    string base = string("result_") + stampBuf;
    fs::path outPath = fs::path(resultsDir) / (base + ".json");
    for (int num = 1; fs::exists(outPath); num++)
        outPath = fs::path(resultsDir) / (base + "_" + to_string(num) + ".json");
    ofstream(outPath) << js.str();
    printf("outputs: %s/\n", outputsDir.c_str());
    printf("saved: %s\n", outPath.string().c_str());
}
