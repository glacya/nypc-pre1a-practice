// NYPC 2026 스텝 업 — 배찌와 다오의 대청소
//
// [보존본] HPA식 계층 분해(HierarchicalSolver)가 들어 있던 버전 (997 435점, result_260929_222815.json).
// 덩어리 사이 상호작용을 다루기 어려워 현재 솔루션(src/solution.cpp)에서는 제거했다. 비교용으로만 유지한다.
//
// stdin으로 입력 하나를 받아, stdout으로 행동 문자열을 출력한다.
// 코드는 세 구역으로 나뉜다.
//   [1] INPUT   : 입력을 읽어 문제 상태(Problem)를 만든다.
//   [2] OUTPUT  : 행동(Action) 표현과, 행동 목록을 출력 형식에 맞게 쓰는 부분.
//   [3] PROCESS : Problem을 받아 행동 목록을 만들어 내는 풀이 로직.
// PROCESS가 INPUT/OUTPUT의 타입을 모두 사용하므로 파일에서는 가장 뒤에 둔다.
// 실행 흐름은 main()에서 INPUT → PROCESS → OUTPUT 순서로 한눈에 보이게 한다.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

// ============================================================================
// [1] INPUT
// ============================================================================

// 격자 위의 좌표. row는 위에서부터, col은 왼쪽에서부터 0-indexed.
struct Pos {
    int row = 0;
    int col = 0;

    bool operator==(const Pos& other) const { return row == other.row && col == other.col; }
    bool operator!=(const Pos& other) const { return !(*this == other); }
};

// 격자 한 칸의 상태. 캐릭터의 시작 위치(B, D)는 빈 칸으로 취급하고,
// 위치 정보는 Character 쪽에 따로 보관한다.
enum class Cell {
    Empty,     // '.', 'B', 'D'
    Obstacle,  // '#': 이동 불가, 물줄기를 막음, 파괴 불가
    Block,     // '@': 이동 불가, 물줄기에 맞으면 빈 칸이 됨
};

// 집의 격자. 칸 상태 조회/변경과 경계·통행 가능 여부 판단을 담당한다.
class Grid {
public:
    Grid() = default;
    Grid(int rows, int cols) : rows_(rows), cols_(cols), cells_(rows * cols, Cell::Empty) {}

    int rows() const { return rows_; }
    int cols() const { return cols_; }

    bool isInside(Pos p) const { return p.row >= 0 && p.row < rows_ && p.col >= 0 && p.col < cols_; }

    Cell at(Pos p) const { return cells_[index(p)]; }
    void set(Pos p, Cell cell) { cells_[index(p)] = cell; }

    // 캐릭터가 서 있거나 이동해 들어갈 수 있는 칸인지 (격자 안의 빈 칸).
    bool isPassable(Pos p) const { return isInside(p) && at(p) == Cell::Empty; }

    // 현재 남아 있는 블럭 수.
    int countBlocks() const {
        int count = 0;
        for (Cell c : cells_)
            if (c == Cell::Block) count++;
        return count;
    }

private:
    int index(Pos p) const { return p.row * cols_ + p.col; }

    int rows_ = 0;
    int cols_ = 0;
    std::vector<Cell> cells_;
};

// 청소에 참여하는 캐릭터 (배찌 또는 다오).
struct Character {
    Pos start;      // 시작 위치
    int power = 0;  // 물줄기 세기 K (물줄기가 나아가는 최대 칸 수)
};

// 입력 전체를 담는 문제 상태.
// characters[0]은 배찌, C=2이면 characters[1]은 다오이며, 행동은 이 순서대로 번갈아 한다.
struct Problem {
    Grid grid;
    std::vector<Character> characters;
};

// 입력 형식:
//   N M C
//   K1 [K2]
//   N줄의 격자 문자열 ('.', '#', '@', 'B', 'D')
Problem readProblem(std::istream& in) {
    int rows = 0, cols = 0, numCharacters = 0;
    in >> rows >> cols >> numCharacters;

    Problem problem;
    problem.characters.resize(numCharacters);
    for (Character& ch : problem.characters) in >> ch.power;

    problem.grid = Grid(rows, cols);
    for (int r = 0; r < rows; r++) {
        std::string line;
        in >> line;
        for (int c = 0; c < cols; c++) {
            Pos p{r, c};
            switch (line[c]) {
                case '#': problem.grid.set(p, Cell::Obstacle); break;
                case '@': problem.grid.set(p, Cell::Block); break;
                case 'B': problem.characters[0].start = p; break;
                case 'D': problem.characters[1].start = p; break;
                default: break;  // '.'
            }
        }
    }
    return problem;
}

// ============================================================================
// [2] OUTPUT
// ============================================================================

// 캐릭터가 한 턴에 하는 행동.
enum class Action {
    Up,     // 'U'
    Down,   // 'D'
    Left,   // 'L'
    Right,  // 'R'
    Bomb,   // 'B': 물폭탄 터트리기
};

// 행동을 출력 문자로 변환한다.
char toChar(Action action) {
    switch (action) {
        case Action::Up: return 'U';
        case Action::Down: return 'D';
        case Action::Left: return 'L';
        case Action::Right: return 'R';
        case Action::Bomb: return 'B';
    }
    return '?';
}

// 행동 목록을 한 줄로 출력한다.
// actions는 출력 형식 그대로의 순서여야 한다: C=1이면 배찌의 행동 순서,
// C=2이면 배찌, 다오, 배찌, 다오, ... 로 번갈아 놓인 순서.
void writeActions(std::ostream& out, const std::vector<Action>& actions) {
    std::string line;
    line.reserve(actions.size());
    for (Action a : actions) line += toChar(a);
    out << line << '\n';
}

// ============================================================================
// [3] PROCESS
// ============================================================================

// ---------------------------------------------------------------------------
// 이동 방향
// ---------------------------------------------------------------------------

// 한 칸 이동을 나타내는 방향: 좌표 변화량과 그에 해당하는 출력 행동.
struct Direction {
    int dRow;
    int dCol;
    Action action;
};

// 네 방향을 "우선순위 순서"로 나열한 목록: U → L → R → D.
// BFS가 이웃을 이 순서로 방문하므로, 같은 거리의 후보가 여럿일 때
// 이 순서에서 먼저 나오는 방향으로 가는 경로가 선택된다.
// 물줄기는 네 방향 모두로 퍼지므로 폭탄 계산에서는 순서가 의미 없다.
constexpr std::array<Direction, 4> kDirections = {{
    {-1, 0, Action::Up},
    {0, -1, Action::Left},
    {0, 1, Action::Right},
    {1, 0, Action::Down},
}};

Pos step(Pos p, const Direction& d) { return Pos{p.row + d.dRow, p.col + d.dCol}; }

// ---------------------------------------------------------------------------
// 게임 상태와 규칙
// ---------------------------------------------------------------------------

// 청소가 진행되는 동안의 상태(현재 격자, 캐릭터 위치, 남은 블럭 수)와
// 문제의 규칙(물줄기 전파, 이동)을 담당한다. 행동을 적용하면 상태가 바뀐다.
class GameState {
public:
    explicit GameState(const Problem& problem)
        : grid_(problem.grid), remainingBlocks_(problem.grid.countBlocks()) {
        for (const Character& ch : problem.characters) {
            positions_.push_back(ch.start);
            powers_.push_back(ch.power);
        }
    }

    const Grid& grid() const { return grid_; }
    int numCharacters() const { return static_cast<int>(positions_.size()); }
    Pos position(int who) const { return positions_[who]; }
    int power(int who) const { return powers_[who]; }
    int remainingBlocks() const { return remainingBlocks_; }

    // 세기 power의 물폭탄을 from에서 터트린다면 부서질 블럭들의 위치 (상태는 바꾸지 않음).
    // 규칙: 네 방향으로 각각 최대 power칸 나아가며, 격자 끝이나 장애물에서 멈추고,
    //       처음 만난 블럭 하나를 부수고 사라진다.
    std::vector<Pos> blastTargetsFrom(Pos from, int power) const {
        std::vector<Pos> targets;
        for (const Direction& d : kDirections) {
            Pos p = from;
            for (int dist = 1; dist <= power; dist++) {
                p = step(p, d);
                if (!grid_.isInside(p) || grid_.at(p) == Cell::Obstacle) break;
                if (grid_.at(p) == Cell::Block) {
                    targets.push_back(p);
                    break;
                }
            }
        }
        return targets;
    }

    // 캐릭터 who가 지금 자기 위치에서 물폭탄을 터트리면 부서질 블럭들의 위치.
    std::vector<Pos> blastTargets(int who) const { return blastTargetsFrom(positions_[who], powers_[who]); }

    // 캐릭터 who의 물폭탄을 적용한다. 부서진 블럭 수를 돌려준다.
    int applyBomb(int who) {
        std::vector<Pos> targets = blastTargets(who);
        for (Pos p : targets) grid_.set(p, Cell::Empty);
        remainingBlocks_ -= static_cast<int>(targets.size());
        return static_cast<int>(targets.size());
    }

    // 캐릭터 who를 방향 d로 한 칸 옮긴다. 호출하는 쪽이 이동 가능함을 보장해야 한다.
    void applyMove(int who, const Direction& d) { positions_[who] = step(positions_[who], d); }

private:
    Grid grid_;
    std::vector<Pos> positions_;
    std::vector<int> powers_;
    int remainingBlocks_;
};

// ---------------------------------------------------------------------------
// 목표 칸 선택: 점수화
// ---------------------------------------------------------------------------

// 후보 칸 점수의 분자(비용 쪽) 공식. 점수가 낮을수록 좋은 목표다.
//   r = 현재 위치에서 후보 칸까지의 보행 거리. 분모(효율 쪽)는 PlannerConfig의 a·x^b.
enum class ScoreFormula {
    // 분자 r: "걸음 수". r = 0(지금 위치)이면 항상 0점이므로,
    //        부술 게 있으면 즉시 폭탄을 쓰는 것과 같다.
    DistancePerBlock,
    // 분자 r + 1: 폭탄 1회도 행동이므로 "행동 수". (b = 1이면) 지금 1개를 부수는 것(1.0)보다
    //        한 칸 옆에서 4개를 부수는 것(0.5)을 더 좋게 본다.
    ActionsPerBlock,
};

// 목표 선택의 조정 가능한 설정값.
struct PlannerConfig {
    ScoreFormula formula = ScoreFormula::ActionsPerBlock;
    // 폭탄 효율 가중치: 분자를 x 대신 a·x^b로 나눈다 (x = 부서질 블럭 수).
    //   b > 1이면 한 번에 여러 개를 부수는 칸을 "행동 수 대비 블럭 수" 이상으로 우대한다.
    //   덩어리 블럭 지도에서 폭탄 한 번의 효율을 높이려는 의도.
    //   a는 모든 후보의 점수에 같은 배율로 곱해지므로(r_z 나누기도 곱셈이라) 순위를 바꾸지 않는다.
    //   다른 항이 덧셈으로 섞이는 공식으로 바뀔 때를 위해 남겨 둔다.
    double blockWeightScale = 1.0;     // a
    double blockWeightExponent = 1.0;  // b
    // 이동 비용 가중치 c: 분자(r 또는 r + 1) 전체를 c제곱한다.
    //   c > 1이면 먼 후보일수록 비용이 거리보다 빠르게 커져, 멀리 걸어가야 하는 목표의
    //   우선순위가 낮아진다. b > 1(여러 개 부수는 칸 우대)이 먼 칸까지 끌고 가는 경향을
    //   상쇄하려는 의도. c = 1이면 원래 공식과 같다.
    //   (r^c + 1 형태도 시험했으나 c = 1.0이 최고여서, 폭탄 1회를 포함한 (r + 1)^c로 교체)
    double distanceExponent = 1.0;     // c
    // C=2 반발 세기 α: 점수를 r_z^α로 나눈다 (0 < α < 1이면 r_z의 영향이 완만해짐).
    double repulsionExponent = 0.5;
};

constexpr PlannerConfig kPlannerConfig{};

// 한 캐릭터의 현재 목표: 목표 칸과, 거기까지 남은 이동 경로.
struct Plan {
    Pos target;
    std::vector<const Direction*> path;  // 앞에서부터 차례로 밟을 방향
    std::size_t nextStep = 0;            // path에서 다음에 쓸 인덱스

    bool arrived() const { return nextStep >= path.size(); }
};

int manhattan(Pos a, Pos b) { return std::abs(a.row - b.row) + std::abs(a.col - b.col); }

// 캐릭터의 다음 목표 후보들을 점수순으로 골라 경로와 함께 돌려준다.
//
// 방법:
//   1. 현재 위치에서 빈 칸만 밟는 BFS로 도달 가능한 모든 칸과 보행 거리 r, 최단 경로를 구한다.
//   2. 각 칸에서 터트렸을 때 부서질 블럭 수 x를 세고, x > 0인 칸만 후보로 삼는다.
//   3. 후보마다 점수를 매겨 낮은 순으로 정렬하고, 앞에서부터 limit개의 경로를 만든다.
//      greedy는 1등만 쓰고(bestTarget), 빔 서치는 상위 여러 개로 가지를 친다.
//        기본 점수 = r^c / (a·x^b) 또는 (r + 1)^c / (a·x^b)  (PlannerConfig::formula, a, b, c)
//        C=2에서 상대에게 목표가 있으면: 기본 점수 / r_z^α
//          r_z = 후보 칸과 상대 목표 칸의 맨해튼 거리.
//          의도: "상대 목표에 가까운 칸은 나쁘게" 평가해 두 캐릭터가 같은 곳으로 몰리는 것을
//          막는다. 나누는 값이 r_z 자체가 아니라 r_z^α(α = 0.5)인 이유는, r_z의 범위(0 ~ 30+)가
//          보행 거리에 비해 커서 그대로 나누면 멀리 흩어지려는 경향이 지나치게 강해지기 때문.
//          r_z = 0(상대 목표와 같은 칸)은 후보에서 제외한다.
//
// 동점 처리: 점수가 같으면 BFS 방문 순서(보행 거리가 짧은 순, 같은 거리면 방향 우선순위 U, L, R, D)가
//   빠른 칸이 앞에 온다. 부동소수 오차로 순서가 흔들리지 않도록 점수를 10^-9 단위로 반올림해 비교한다.
// 알려진 한계:
//   - r_z는 칸 사이 거리라서, K가 클 때 서로 다른 방향에서 같은 블럭을 노리는 경우를 잡지 못한다.
//   - 한 번에 하나의 목표만 보는 근시안적 선택이다 (이후의 순서는 고려하지 않음).
// 후보가 없으면 빈 목록.
std::vector<Plan> rankTargets(const GameState& state, int who, const std::optional<Pos>& partnerTarget,
                              const PlannerConfig& config, int limit) {
    const Grid& grid = state.grid();
    const Pos start = state.position(who);

    // BFS: distance[r][c] = 보행 거리 (미방문 -1), parent[r][c] = 그 칸으로 들어올 때 쓴 방향.
    std::vector<std::vector<int>> distance(grid.rows(), std::vector<int>(grid.cols(), -1));
    std::vector<std::vector<const Direction*>> parent(grid.rows(),
                                                      std::vector<const Direction*>(grid.cols(), nullptr));
    std::vector<Pos> visitOrder;
    std::queue<Pos> queue;
    distance[start.row][start.col] = 0;
    queue.push(start);
    while (!queue.empty()) {
        Pos cur = queue.front();
        queue.pop();
        visitOrder.push_back(cur);
        for (const Direction& d : kDirections) {
            Pos next = step(cur, d);
            if (!grid.isPassable(next) || distance[next.row][next.col] != -1) continue;
            distance[next.row][next.col] = distance[cur.row][cur.col] + 1;
            parent[next.row][next.col] = &d;
            queue.push(next);
        }
    }

    // 후보 평가
    struct Candidate {
        long long scoreKey;  // 점수 × 10^9 반올림 (작을수록 좋음)
        Pos cell;
    };
    std::vector<Candidate> candidates;
    for (Pos cell : visitOrder) {
        int blocks = static_cast<int>(state.blastTargetsFrom(cell, state.power(who)).size());
        if (blocks == 0) continue;

        double r = distance[cell.row][cell.col];
        double cost = std::pow((config.formula == ScoreFormula::DistancePerBlock) ? r : r + 1, config.distanceExponent);
        double efficiency = config.blockWeightScale * std::pow(static_cast<double>(blocks), config.blockWeightExponent);
        double score = cost / efficiency;
        if (partnerTarget) {
            int rz = manhattan(cell, *partnerTarget);
            if (rz == 0) continue;  // 상대 목표와 같은 칸은 제외
            score /= std::pow(static_cast<double>(rz), config.repulsionExponent);
        }
        candidates.push_back({std::llround(score * 1e9), cell});
    }
    // visitOrder 순서로 넣었으므로 stable_sort가 동점을 BFS 순서대로 유지한다.
    std::stable_sort(candidates.begin(), candidates.end(),
                     [](const Candidate& x, const Candidate& y) { return x.scoreKey < y.scoreKey; });
    if (static_cast<int>(candidates.size()) > limit) candidates.resize(limit);

    // 경로 복원: 목표에서 parent를 따라 거슬러 올라간 뒤 뒤집는다.
    std::vector<Plan> plans;
    for (const Candidate& c : candidates) {
        Plan plan;
        plan.target = c.cell;
        for (Pos p = c.cell; p != start;) {
            const Direction* d = parent[p.row][p.col];
            plan.path.push_back(d);
            p = Pos{p.row - d->dRow, p.col - d->dCol};
        }
        std::reverse(plan.path.begin(), plan.path.end());
        plans.push_back(std::move(plan));
    }
    return plans;
}

// ---------------------------------------------------------------------------
// 턴 진행
// ---------------------------------------------------------------------------

// 게임을 한 턴씩 진행하며 행동 목록을 쌓는다. 각 캐릭터의 목표(Plan)를 보관한다.
// 값 복사가 가능해서, 빔 서치가 진행 상황을 통째로 복제해 여러 갈래로 이어 갈 수 있다.
//
// 턴 규칙:
//   1. 차례인 캐릭터에게 목표가 없으면 "결정이 필요한" 상태다 (needsTarget).
//      호출하는 쪽이 candidateTargets 중 하나를 골라 assignTarget으로 정해 준다.
//      greedy는 항상 1등을, 빔 서치는 여러 후보를 각각 시도한다.
//   2. playTurn: 목표 칸에 도착해 있거나 목표가 없으면(후보가 없어 할 일이 없음) 폭탄,
//      아니면 저장된 경로대로 한 칸 이동한다.
//      후보가 없는 경우는 C=2에서 남은 블럭이 상대 목표에만 걸린 경우 등이며,
//      "제자리 대기" 행동이 없으므로 폭탄으로 턴을 버린다.
//   3. 폭탄으로 블럭이 하나라도 부서지면 모든 캐릭터의 목표를 지운다.
//      격자가 바뀌어 기존 점수가 더 이상 맞지 않기 때문이다 (다음 결정 때 다시 BFS).
//      반대로 블럭이 부서지지 않는 동안은 격자가 그대로이므로 목표를 유지한다.
//      블럭이 부서져도 새로 막히는 칸은 없으므로, 저장한 경로가 불법 이동이 되는 일은 없다.
class TurnRunner {
public:
    TurnRunner(const Problem& problem, const PlannerConfig& config)
        : state_(problem), config_(config), plans_(problem.characters.size()) {}

    bool finished() const { return state_.remainingBlocks() == 0; }
    const std::vector<Action>& actions() const { return actions_; }

    // 이번 턴에 행동할 캐릭터 (배찌부터 번갈아).
    int currentCharacter() const { return static_cast<int>(actions_.size() % plans_.size()); }

    // 이번 턴의 캐릭터에게 새 목표를 정해 줘야 하는가.
    bool needsTarget() const { return !finished() && !plans_[currentCharacter()]; }

    // 이번 턴의 캐릭터가 고를 수 있는 목표 후보 (점수순 상위 limit개).
    std::vector<Plan> candidateTargets(int limit) const {
        const int who = currentCharacter();
        return rankTargets(state_, who, partnerTarget(who), config_, limit);
    }

    // 이번 턴의 캐릭터에게 목표를 정해 준다.
    void assignTarget(Plan plan) { plans_[currentCharacter()] = std::move(plan); }

    // 이번 턴의 캐릭터가 한 턴 행동한다 (위의 턴 규칙 2, 3).
    void playTurn() {
        const int who = currentCharacter();
        std::optional<Plan>& plan = plans_[who];
        if (!plan || plan->arrived()) {
            // 목표 칸에 도착했거나(유효한 폭탄) 할 일이 없음(턴 버리기)
            if (state_.applyBomb(who) > 0) clearAllPlans();
            actions_.push_back(Action::Bomb);
            return;
        }
        const Direction* d = plan->path[plan->nextStep++];
        state_.applyMove(who, *d);
        actions_.push_back(d->action);
    }

    // 이후의 진행을 완전히 결정하는 정보를 문자열로 만든 키. 빔 서치의 중복 제거에 쓴다.
    // 포함: 남은 블럭 배치, 캐릭터별 위치, 캐릭터별 목표와 남은 경로, 다음 차례인 캐릭터.
    // 지금까지의 행동 수는 포함하지 않는다 (같은 키라면 행동 수가 적은 쪽이 항상 낫다).
    // 남은 경로까지 넣는 이유: 같은 위치·같은 목표라도 목표를 정한 위치가 달랐다면 경로가 다를 수 있고,
    // 경로가 다르면 C=2에서 두 캐릭터가 만나는 타이밍 등 이후 전개가 달라질 수 있기 때문이다.
    std::string stateKey() const {
        const Grid& grid = state_.grid();
        std::string key;
        key.reserve(grid.rows() * grid.cols() + 64);
        for (int r = 0; r < grid.rows(); r++)
            for (int c = 0; c < grid.cols(); c++) key += (grid.at(Pos{r, c}) == Cell::Block) ? '@' : '.';
        key += '|';
        key += static_cast<char>('0' + currentCharacter());
        for (int who = 0; who < state_.numCharacters(); who++) {
            const Pos p = state_.position(who);
            key += '|' + std::to_string(p.row) + ',' + std::to_string(p.col);
            const std::optional<Plan>& plan = plans_[who];
            if (!plan) {
                key += ":-";
                continue;
            }
            key += ':' + std::to_string(plan->target.row) + ',' + std::to_string(plan->target.col) + ':';
            for (std::size_t i = plan->nextStep; i < plan->path.size(); i++) key += toChar(plan->path[i]->action);
        }
        return key;
    }

    // 결정이 필요할 때마다 1등 후보를 고르며 끝까지(또는 행동 수 maxActions까지) 진행한다.
    void runGreedyToEnd(int maxActions) {
        while (!finished() && static_cast<int>(actions_.size()) < maxActions) {
            if (needsTarget()) {
                std::vector<Plan> best = candidateTargets(1);
                if (!best.empty()) assignTarget(std::move(best.front()));
            }
            playTurn();
        }
    }

private:
    // who의 상대 캐릭터의 현재 목표 칸. C=1이거나 상대에게 목표가 없으면 std::nullopt.
    std::optional<Pos> partnerTarget(int who) const {
        if (state_.numCharacters() < 2) return std::nullopt;
        const std::optional<Plan>& other = plans_[1 - who];
        if (!other) return std::nullopt;
        return other->target;
    }

    void clearAllPlans() {
        for (std::optional<Plan>& plan : plans_) plan.reset();
    }

    GameState state_;
    PlannerConfig config_;
    std::vector<std::optional<Plan>> plans_;
    std::vector<Action> actions_;
};

// 출력 길이 제한. 이보다 긴 행동 열은 만들지 않는다.
constexpr int kMaxActions = 100000;

// 목표 칸 점수화 greedy로 행동 목록을 만든다.
// 배찌부터 캐릭터들이 번갈아 한 턴씩 행동하며, 블럭이 모두 없어지면 즉시 멈춘다
// (C=2에서 배찌의 턴에 끝나도 된다). 출력 길이 제한에 도달하면 그 자리에서 멈춘다.
std::vector<Action> solveGreedy(const Problem& problem) {
    TurnRunner runner(problem, kPlannerConfig);
    runner.runGreedyToEnd(kMaxActions);
    return runner.actions();
}

// ---------------------------------------------------------------------------
// 빔 서치: 목표 선택 지점에서 가지를 치고 greedy rollout으로 평가
// ---------------------------------------------------------------------------

// 빔 서치 설정값.
struct BeamConfig {
    int width = 12;                  // X: 단계마다 남기는 후보(진행 상황) 수
    int branching = 8;               // 결정 지점마다 시도하는 목표 후보 수 (greedy 점수 상위)
    double timeLimitSeconds = 10.0;  // 시간 제한. 넘으면 그때까지 찾은 최선의 해를 돌려준다.
};

constexpr BeamConfig kBeamConfig{};

// greedy의 근시안(지금 가장 좋아 보이는 목표 하나만 고름)을 완화하기 위한 빔 서치.
//
// 탐색 단위 (한 단계 = 결정 하나):
//   이동 하나하나를 단계로 삼으면 이동만으로는 상태가 좋아졌는지 판단하기 어렵다.
//   그래서 greedy가 "새 목표를 정하는 순간"만 분기점으로 삼는다. 각 빔 노드는 다음 분기점까지
//   턴을 진행한 뒤, greedy 점수 상위 branching개의 목표로 각각 갈라진다.
//   C=2도 TurnRunner가 차례를 관리하므로 같은 방식으로 처리된다 (분기점마다 그 순간 차례인 캐릭터가 고름).
//
// 평가 (rollout):
//   갈라진 자식을 복제해 greedy로 끝까지 진행해 본 총 행동 수로 평가한다. 작을수록 좋다.
//   "지금까지의 비용 + 남은 비용 추정"을 따로 설계하지 않아도 되고, 단계마다 진행 정도가 다른
//   노드들(부순 블럭 수, 행동 수가 제각각)을 같은 기준(완성된 해의 길이)으로 비교할 수 있다.
//   rollout 자체가 완성된 올바른 해이므로, 지금까지 본 rollout 중 가장 짧은 것을 답으로 보관한다.
//   따라서 결과는 항상 greedy 이하다 (첫 rollout이 곧 greedy 해).
//
// 중복 제거: 목표를 정한 직후의 자식마다 상태 키(TurnRunner::stateKey)를 만든다. 같은 키라면 이후 진행이
//   똑같고 비용만 행동 수 차이만큼 다르므로, 행동 수가 적은 쪽만 의미가 있다. 두 단계로 걸러 낸다.
//   - 빔에 들어갔던 상태 (탐색 전체, beamVisited_): 같은 키가 이전에 같거나 적은 행동 수로 빔에
//     들어간 적이 있으면 버린다. 그 상태의 후손은 이미 탐색 중이거나 탐색했기 때문이다.
//   - 같은 단계의 자식들 (layerSeen): 같은 단계에서 같은 키가 같거나 적은 행동 수로 이미 나왔으면 버린다.
//     빔 자리를 같은 상태가 여러 번 차지하는 것을 막는다.
//   빔에 뽑히지 못한 상태는 기록하지 않는다. 같은 상태가 나중 단계에서 다시 나오면 그때의 경쟁자들
//   사이에서 다시 평가받을 수 있다. (처음에는 만든 자식을 모두 기록했으나, 버려진 상태가 다시 경쟁할
//   기회를 막아 13번이 32 → 33으로 나빠져 이 방식으로 바꿈)
//   탐색하는 상태는 대략 단계 수 × width × branching개뿐이라 해시 테이블로 전부 관리해도 부담이 없다.
// 선택: 모든 자식을 rollout 비용순으로 정렬해 상위 width개를 다음 빔으로 남긴다 (동점이면 생성 순서).
//
// 한계:
//   - 평가가 greedy rollout이라, greedy가 잘 못 보는 전개는 좋은 자식이어도 낮게 평가될 수 있다.
//   - 비용: 단계마다 width × branching번 rollout. 큰 지도에서는 시간 제한에 걸려 일찍 끝날 수 있다.
class BeamSearch {
    // 한 단계에서 만든 자식: greedy rollout 비용, 상태 키, 진행 상황.
    struct Child {
        std::size_t rolloutCost;
        std::string key;
        TurnRunner runner;
    };

public:
    BeamSearch(const Problem& problem, const PlannerConfig& planner, const BeamConfig& config)
        : problem_(problem), planner_(planner), config_(config) {}

    // 중복이라 버린 자식 수와, 빔에 들어간 서로 다른 상태 수. 진단용.
    std::size_t duplicatesSkipped() const { return duplicatesSkipped_; }
    std::size_t distinctStates() const { return beamVisited_.size(); }

    std::vector<Action> run() {
        startTime_ = std::chrono::steady_clock::now();

        TurnRunner root(problem_, planner_);
        best_ = rollout(root);  // greedy 해: 빔이 이보다 나빠지지 않도록 기준으로 둔다

        std::vector<TurnRunner> beam = {root};
        while (!beam.empty() && !timeUp()) {
            // 이번 단계의 자식들과, 이번 단계에서 나온 상태 키 → 최소 행동 수
            std::vector<Child> children;
            std::unordered_map<std::string, std::size_t> layerSeen;
            for (TurnRunner& node : beam) {
                std::vector<Plan> choices = advanceToDecision(node);
                if (node.finished()) {
                    record(node.actions());
                    continue;
                }
                for (Plan& plan : choices) {
                    if (timeUp()) break;
                    TurnRunner child = node;
                    child.assignTarget(std::move(plan));
                    std::string key = child.stateKey();
                    const std::size_t actionCount = child.actions().size();
                    if (seenWithFewerActions(beamVisited_, key, actionCount) ||
                        seenWithFewerActions(layerSeen, key, actionCount)) {
                        duplicatesSkipped_++;
                        continue;
                    }
                    layerSeen[key] = actionCount;
                    const std::size_t cost = rollout(child).size();
                    children.push_back({cost, std::move(key), std::move(child)});
                }
            }
            std::stable_sort(children.begin(), children.end(),
                             [](const Child& x, const Child& y) { return x.rolloutCost < y.rolloutCost; });
            if (static_cast<int>(children.size()) > config_.width)
                children.erase(children.begin() + config_.width, children.end());

            beam.clear();
            for (Child& child : children) {
                // 빔에 들어간 상태만 탐색 전체의 기록에 올린다.
                beamVisited_[child.key] = child.runner.actions().size();
                beam.push_back(std::move(child.runner));
            }
        }
        return best_;
    }

private:
    // node를 다음 분기점(차례인 캐릭터가 새 목표를 골라야 하고, 후보가 하나 이상 있음)까지 진행하고,
    // 그 분기점의 목표 후보들을 돌려준다. 끝까지 가 버리면(또는 길이 제한) 빈 목록.
    // 후보가 하나도 없는 결정(할 일 없음)은 분기점이 아니므로 그냥 턴을 진행한다.
    std::vector<Plan> advanceToDecision(TurnRunner& node) const {
        while (!node.finished() && static_cast<int>(node.actions().size()) < kMaxActions) {
            if (node.needsTarget()) {
                std::vector<Plan> choices = node.candidateTargets(config_.branching);
                if (!choices.empty()) return choices;
            }
            node.playTurn();
        }
        return {};
    }

    // node를 복제해 greedy로 끝까지 진행한 해. 지금까지의 최선보다 짧으면 기록한다.
    std::vector<Action> rollout(const TurnRunner& node) {
        TurnRunner copy = node;
        copy.runGreedyToEnd(kMaxActions);
        if (copy.finished()) record(copy.actions());
        return copy.actions();
    }

    void record(const std::vector<Action>& actions) {
        if (best_.empty() || actions.size() < best_.size()) best_ = actions;
    }

    // table에 key가 행동 수 actionCount 이하로 이미 기록돼 있는가.
    static bool seenWithFewerActions(const std::unordered_map<std::string, std::size_t>& table,
                                     const std::string& key, std::size_t actionCount) {
        auto it = table.find(key);
        return it != table.end() && it->second <= actionCount;
    }

    bool timeUp() const {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime_).count() >
               config_.timeLimitSeconds;
    }

    const Problem& problem_;
    PlannerConfig planner_;
    BeamConfig config_;
    std::chrono::steady_clock::time_point startTime_;
    std::vector<Action> best_;
    std::unordered_map<std::string, std::size_t> beamVisited_;  // 빔에 들어간 상태 키 → 최소 행동 수
    std::size_t duplicatesSkipped_ = 0;
};

// ---------------------------------------------------------------------------
// 정확한 탐색: (부순 블럭 집합 S, 캐릭터 위치들, 차례) 상태 공간 BFS
// ---------------------------------------------------------------------------

// 정확한 탐색의 한계값. 하나라도 넘으면 탐색을 포기하고 greedy를 쓴다.
struct ExactSearchLimits {
    int maxBlocks = 16;                        // 블럭 수 n이 이보다 크면 시도하지 않음 (n = 19~28은 10초 안에 불가능함을 확인)
    double timeLimitSeconds = 10.0;            // BFS 실행 시간 제한
    std::uint64_t maxStates = 200'000'000ULL;  // 상태 배열 크기 상한 (메모리 보호: 상태당 5바이트 → 약 1GB)
};

constexpr ExactSearchLimits kExactSearchLimits{};

// 최단 행동 열을 상태 공간 BFS로 구한다.
//
// 핵심 관찰: 지금까지 부순 블럭 집합 S가 정해지면 현재 격자는 "초기 격자에서 S의 블럭만
//   빈 칸으로 바꾼 것"으로 완전히 결정된다. 따라서 블럭이 부서지며 길과 물줄기 범위가
//   바뀌는 "동적 그래프" 문제가, 상태에 S를 넣는 것만으로 정적인 그래프 탐색이 된다.
//
// 상태: (S, 캐릭터별 위치, 차례)
//   - S: n비트 마스크. 블럭 i가 부서졌으면 i번째 비트가 1.
//   - 위치: 장애물이 아닌 칸(빈 칸 + 블럭 칸)에 번호를 붙여 사용. 블럭 칸은 부서진 뒤에만 설 수 있다.
//   - 차례: C=2에서 다음에 행동할 캐릭터 (0 = 배찌, 1 = 다오). C=1이면 항상 0.
//     같은 (S, 위치)라도 누구 차례인지에 따라 이후가 달라지므로 상태에 포함해야 한다.
// 전이: 차례인 캐릭터의 행동 하나 (비용 1).
//   - 이동: 격자 안이고 장애물이 아니며, 블럭 칸이면 이미 부서진 경우만.
//   - 폭탄: 현재 S 기준으로 물줄기를 계산해 부서진 블럭을 S에 추가.
//     C=1에서 아무것도 부수지 못하는 폭탄은 최적해에 나올 수 없으므로 전이에서 뺀다.
//     C=2에서는 "대기" 행동이 없으므로, 헛폭탄이 상대를 기다리는 유일한 방법이 될 수 있어 포함한다.
// 모든 행동의 비용이 1이므로 BFS로 처음 도달한 "S = 전체" 상태가 최적해다.
//
// 저장: 상태를 정수 인덱스로 펼쳐, 가능한 모든 상태(2^n × 칸^C × C개)의 자리를 배열로 미리 잡는다.
//   - parent_ / lastAction_: 상태 인덱스 → 부모 상태 인덱스(4바이트), 그 상태로 올 때의 행동(1바이트).
//   - queue_: BFS 큐 (방문 순서대로 상태 인덱스를 쌓음).
//   실제로 방문하는 상태는 이보다 훨씬 적지만(예: 덩어리 안쪽 블럭은 바깥 블럭보다 먼저 부서질 수
//   없으므로 그런 S는 나타나지 않는다), 구현이 단순하고 방문 확인이 빠르다.
// 한계: 상태 수가 n과 C에 대해 지수적이다 (C=2는 칸 수의 제곱이 곱해짐).
//   상태 배열 크기나 시간 한계를 넘으면 포기한다.
//   (방문한 상태만 해시 테이블에 저장하는 방식도 시험했으나, 블럭 19~28개 케이스는 그래도
//    10초 안에 끝나지 않아 단순한 배열 방식으로 되돌렸다.)
class ExactSearch {
public:
    ExactSearch(const Problem& problem, const ExactSearchLimits& limits) : problem_(problem), limits_(limits) {
        const Grid& grid = problem.grid;
        cellIndex_.assign(grid.rows() * grid.cols(), -1);
        blockIndex_.assign(grid.rows() * grid.cols(), -1);
        for (int r = 0; r < grid.rows(); r++) {
            for (int c = 0; c < grid.cols(); c++) {
                Pos p{r, c};
                if (grid.at(p) == Cell::Obstacle) continue;
                cellIndex_[r * grid.cols() + c] = static_cast<int>(cells_.size());
                cells_.push_back(p);
                if (grid.at(p) == Cell::Block) blockIndex_[r * grid.cols() + c] = numBlocks_++;
            }
        }
        numCharacters_ = static_cast<int>(problem.characters.size());
    }

    // 탐색이 끝났을 때(성공/포기 모두) 방문한 상태 수. 진단용.
    std::size_t visitedStates() const { return queue_.size(); }

    // 포기한 이유 (성공했으면 빈 문자열). 진단용.
    const std::string& giveUpReason() const { return giveUpReason_; }

    // 최단 행동 열을 구한다. 한계값을 넘으면 std::nullopt.
    std::optional<std::vector<Action>> run() {
        std::vector<std::vector<Action>> found = searchGoals(1);
        if (found.empty()) return std::nullopt;
        return found.front();
    }

    // 모든 블럭을 부수는 서로 다른 목표 상태를 짧은 순으로 최대 maxGoals개 찾아 각각의 행동 열을 돌려준다.
    // BFS는 거리 순으로 상태를 발견하므로, 처음 발견한 maxGoals개가 곧 가장 짧은 maxGoals개다.
    // C=1에서 목표 상태는 "마지막 폭탄을 쓴 위치"로 구분되므로, 끝나는 위치가 서로 다른 해들이 된다.
    // (계층 분해에서 덩어리를 어디서 끝낼지 고를 수 있게 하려는 용도)
    // 한계값을 넘거나 해가 없으면 그때까지 찾은 것만 돌려준다 (빈 목록일 수 있음).
    std::vector<std::vector<Action>> searchGoals(int maxGoals) {
        std::vector<std::vector<Action>> found;
        if (numBlocks_ > limits_.maxBlocks) return giveUp("too many blocks"), found;
        if (numBlocks_ == 0) return {std::vector<Action>{}};  // 부술 블럭이 없음

        // 상태 수 = 2^n × 칸^C × C. 한계를 넘으면 배열을 잡지 않고 포기한다.
        std::uint64_t total = std::uint64_t{1} << numBlocks_;
        for (int i = 0; i < numCharacters_; i++) total *= cells_.size();
        total *= numCharacters_;
        if (total > limits_.maxStates) return giveUp("state limit"), found;
        parent_.assign(total, kUnvisited);
        lastAction_.assign(total, 0);

        const std::uint32_t fullMask =
            (numBlocks_ == 32) ? UINT32_MAX : (std::uint32_t{1} << numBlocks_) - 1;
        StateFields startFields;
        for (int i = 0; i < numCharacters_; i++) startFields.cells[i] = cellOf(problem_.characters[i].start);
        const std::uint32_t start = static_cast<std::uint32_t>(encode(startFields));
        parent_[start] = start;  // 시작 상태는 자기 자신을 부모로 표시
        queue_.push_back(start);

        const auto startTime = std::chrono::steady_clock::now();
        for (std::size_t head = 0; head < queue_.size(); head++) {
            // 시간 제한 확인 (매번 시계를 읽지 않도록 4096번에 한 번)
            if ((head & 4095) == 0 && elapsedSeconds(startTime) > limits_.timeLimitSeconds)
                return giveUp("time limit"), found;

            const std::uint32_t cur = queue_[head];
            const StateFields f = decode(cur);
            const int who = f.turn;
            const int nextTurn = (f.turn + 1) % numCharacters_;

            // 폭탄 전이
            const std::uint32_t blasted = blastMask(cells_[f.cells[who]], problem_.characters[who].power, f.destroyed);
            if (blasted != 0 || numCharacters_ == 2) {
                StateFields nf = f;
                nf.destroyed = f.destroyed | blasted;
                nf.turn = nextTurn;
                const std::uint32_t next = static_cast<std::uint32_t>(encode(nf));
                if (nf.destroyed == fullMask) {
                    // 목표 상태: 기록만 하고 큐에는 넣지 않는다 (이후 행동은 필요 없음).
                    if (parent_[next] == kUnvisited) {
                        parent_[next] = cur;
                        lastAction_[next] = static_cast<std::uint8_t>(Action::Bomb);
                        found.push_back(reconstruct(start, next));
                        if (static_cast<int>(found.size()) >= maxGoals) return found;
                    }
                } else {
                    visit(next, cur, Action::Bomb);
                }
            }

            // 이동 전이
            for (const Direction& d : kDirections) {
                const int target = walkableCell(step(cells_[f.cells[who]], d), f.destroyed);
                if (target < 0) continue;
                StateFields nf = f;
                nf.cells[who] = target;
                nf.turn = nextTurn;
                visit(static_cast<std::uint32_t>(encode(nf)), cur, d.action);
            }
        }
        if (found.empty()) giveUp("unreachable");  // 입력이 올바르다면 도달하지 않음 (모든 블럭은 파괴 가능)
        return found;
    }

private:
    // 상태를 필드별로 풀어 놓은 형태.
    struct StateFields {
        std::uint32_t destroyed = 0;        // S
        std::array<int, 2> cells = {0, 0};  // 캐릭터별 칸 번호 (C=1이면 [0]만 사용)
        int turn = 0;                       // 다음에 행동할 캐릭터
    };

    static constexpr std::uint32_t kUnvisited = UINT32_MAX;

    // 인덱스 = ((S × 칸 수 + 위치0) × 칸 수 + 위치1) × C + 차례
    // 상태 수 상한(maxStates)이 2^32보다 작으므로 저장은 32비트로 충분하다.
    std::uint64_t encode(const StateFields& f) const {
        std::uint64_t key = f.destroyed;
        for (int i = 0; i < numCharacters_; i++) key = key * cells_.size() + f.cells[i];
        return key * numCharacters_ + f.turn;
    }

    StateFields decode(std::uint64_t key) const {
        StateFields f;
        f.turn = static_cast<int>(key % numCharacters_);
        key /= numCharacters_;
        for (int i = numCharacters_ - 1; i >= 0; i--) {
            f.cells[i] = static_cast<int>(key % cells_.size());
            key /= cells_.size();
        }
        f.destroyed = static_cast<std::uint32_t>(key);
        return f;
    }

    // 처음 보는 상태면 부모·행동을 기록하고 큐에 넣은 뒤 true, 이미 방문했으면 false.
    bool visit(std::uint32_t state, std::uint32_t parentState, Action action) {
        if (parent_[state] != kUnvisited) return false;
        parent_[state] = parentState;
        lastAction_[state] = static_cast<std::uint8_t>(action);
        queue_.push_back(state);
        return true;
    }

    void giveUp(const char* reason) { giveUpReason_ = reason; }

    int cellOf(Pos p) const { return cellIndex_[p.row * problem_.grid.cols() + p.col]; }

    // 부순 블럭 집합이 destroyed일 때 p에 설 수 있으면 그 칸 번호, 아니면 -1.
    int walkableCell(Pos p, std::uint32_t destroyed) const {
        if (!problem_.grid.isInside(p)) return -1;
        const int cell = cellOf(p);
        if (cell < 0) return -1;  // 장애물
        const int block = blockIndex_[p.row * problem_.grid.cols() + p.col];
        if (block >= 0 && !(destroyed >> block & 1)) return -1;  // 아직 남은 블럭
        return cell;
    }

    // 부순 블럭 집합이 destroyed일 때 from에서 세기 power로 터트리면 새로 부서지는 블럭들의 마스크.
    // 규칙은 GameState::blastTargetsFrom과 같다 (장애물/격자 끝에서 멈춤, 방향마다 첫 블럭 하나).
    std::uint32_t blastMask(Pos from, int power, std::uint32_t destroyed) const {
        std::uint32_t mask = 0;
        for (const Direction& d : kDirections) {
            Pos p = from;
            for (int dist = 1; dist <= power; dist++) {
                p = step(p, d);
                if (!problem_.grid.isInside(p) || problem_.grid.at(p) == Cell::Obstacle) break;
                const int block = blockIndex_[p.row * problem_.grid.cols() + p.col];
                if (block >= 0 && !(destroyed >> block & 1)) {
                    mask |= std::uint32_t{1} << block;
                    break;
                }
            }
        }
        return mask;
    }

    // 목표 상태에서 부모를 따라 시작 상태까지 거슬러 올라가며 행동을 모은 뒤 뒤집는다.
    std::vector<Action> reconstruct(std::uint32_t start, std::uint32_t goal) const {
        std::vector<Action> actions;
        for (std::uint32_t s = goal; s != start; s = parent_[s]) actions.push_back(static_cast<Action>(lastAction_[s]));
        std::reverse(actions.begin(), actions.end());
        return actions;
    }

    static double elapsedSeconds(std::chrono::steady_clock::time_point since) {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - since).count();
    }

    const Problem& problem_;
    ExactSearchLimits limits_;
    std::vector<Pos> cells_;       // 칸 번호 → 좌표 (장애물이 아닌 칸만)
    std::vector<int> cellIndex_;   // 좌표 → 칸 번호 (장애물은 -1)
    std::vector<int> blockIndex_;  // 좌표 → 블럭 번호 (블럭이 아니면 -1)
    int numBlocks_ = 0;
    int numCharacters_ = 1;

    std::vector<std::uint32_t> parent_;     // 상태 인덱스 → 부모 상태 인덱스 (미방문 kUnvisited)
    std::vector<std::uint8_t> lastAction_;  // 상태 인덱스 → 그 상태로 올 때의 행동
    std::vector<std::uint32_t> queue_;      // BFS 큐 (방문 순서)
    std::string giveUpReason_;
};

// ---------------------------------------------------------------------------
// 계층 분해 (HPA* 방식): 블럭 덩어리 단위로 나눠 덩어리 안은 정확히, 덩어리 사이는 순서 DP로
// ---------------------------------------------------------------------------

// 계층 분해 설정값.
struct HierarchicalConfig {
    int maxClusterBlocks = 10;       // 덩어리 하나의 블럭 수 상한 (넘으면 이 방법을 쓰지 않음)
    int maxClusters = 14;            // 덩어리 수 상한 (DP 상태가 2^덩어리 수)
    int exitsPerState = 8;           // DP 상태(치운 덩어리 집합)마다 남기는 "현재 위치" 후보 수
    double timeLimitSeconds = 10.0;  // 시간 제한. 넘으면 포기한다.
};

constexpr HierarchicalConfig kHierarchicalConfig{};

// 블럭 덩어리를 따로 풀어 이어 붙이는 C=1 전용 풀이. HPA*(Hierarchical Path-Finding A*)의
// "구역 안 비용은 미리 정확히, 구역 사이는 추상 그래프에서 탐색" 구조를 따른다.
//
// 1. 덩어리 식별: 상하좌우로 붙은 블럭끼리 한 덩어리 (4방향 연결 요소, flood fill).
// 2. 덩어리 안 (정확): 덩어리 Y를 치우는 부분 문제를 상태 공간 BFS(ExactSearch)로 푼다.
//    부분 문제의 지도: 이미 치운 덩어리의 칸은 빈 칸, 아직 안 치운 다른 덩어리의 블럭은 장애물,
//    Y의 블럭만 블럭. 시작 위치는 현재 위치이므로, 덩어리까지 걸어가는 비용도 함께 최적화된다.
//    끝나는 위치(마지막 폭탄 위치)가 다음 덩어리까지의 거리를 좌우하므로, 끝나는 위치가 서로 다른
//    짧은 해를 여러 개(exitsPerState개) 받아 둔다.
// 3. 덩어리 사이 (순서): (치운 덩어리 집합, 현재 위치)에 대한 DP. 집합을 작은 것부터 보며
//    다음에 치울 덩어리를 하나씩 붙인다 (Held–Karp와 같은 모양). 위치 후보가 폭증하지 않도록
//    집합마다 행동 수가 적은 위치 exitsPerState개만 남긴다 (이 부분은 근사).
//
// 올바름: 부분 문제는 "안 치운 다른 덩어리 = 장애물"로 가정하지만, 실제로는 그 블럭이 물줄기에 맞아
//   먼저 부서질 수 있다. 이 차이는 실제 격자를 부분 문제의 격자보다 "빈 칸이 같거나 많게"만 만든다
//   (부분 문제의 물줄기가 지나는 칸은 실제로도 비어 있고, 부분 문제가 부수는 블럭은 실제로도 부서지거나
//   이미 부서져 있다). 따라서 이어 붙인 행동 열의 이동은 실제로도 항상 합법이고, 모든 덩어리를 치우면
//   실제로도 모든 블럭이 없어진다. 마지막에 실제로 시뮬레이션해 확인하고, 일찍 끝나면 그 자리에서 자른다.
//
// 한계:
//   - 덩어리 경계의 상호작용(한 폭탄으로 두 덩어리를 동시에 맞히기, 덩어리를 반쯤 치우고 다른 곳에
//     다녀오기)은 계획에 반영되지 않는다. 우연히 생긴 이득은 실제 시뮬레이션에서만 드러난다.
//   - 부분 문제에서 안 치운 덩어리를 장애물로 보므로, 그 사이를 물줄기로 관통하는 해는 찾지 못한다.
//   - C=2는 두 캐릭터가 덩어리를 나눠 맡는 문제가 추가로 필요해 다루지 않는다.
class HierarchicalSolver {
public:
    HierarchicalSolver(const Problem& problem, const HierarchicalConfig& config)
        : problem_(problem), config_(config) {}

    // 덩어리 수. 진단용 (run 이후 유효).
    int numClusters() const { return static_cast<int>(clusters_.size()); }

    // 포기한 이유 (성공했으면 빈 문자열). 진단용.
    const std::string& giveUpReason() const { return giveUpReason_; }

    std::optional<std::vector<Action>> run() {
        if (problem_.characters.size() != 1) return giveUp("C=2 not supported");
        findClusters();
        if (static_cast<int>(clusters_.size()) > config_.maxClusters) return giveUp("too many clusters");
        for (const std::vector<Pos>& cluster : clusters_)
            if (static_cast<int>(cluster.size()) > config_.maxClusterBlocks) return giveUp("cluster too large");

        const auto startTime = std::chrono::steady_clock::now();
        const int numClusters = static_cast<int>(clusters_.size());
        const int fullSet = (1 << numClusters) - 1;

        // best[set]: 덩어리 집합 set을 치운 뒤의 (현재 위치, 여기까지의 행동 열) 후보들 (행동 수 오름차순)
        std::vector<std::vector<Partial>> best(1 << numClusters);
        best[0].push_back({problem_.characters[0].start, {}});

        // set에 덩어리를 더하면 값이 커지므로, set을 오름차순으로 보면 항상 완성된 상태에서 확장한다.
        for (int set = 0; set < fullSet; set++) {
            for (const Partial& partial : best[set]) {
                for (int next = 0; next < numClusters; next++) {
                    if (set >> next & 1) continue;
                    if (std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count() >
                        config_.timeLimitSeconds)
                        return giveUp("time limit");

                    for (std::vector<Action>& clearing : clearCluster(set, next, partial.position)) {
                        Partial extended{finalPosition(partial.position, clearing), partial.actions};
                        extended.actions.insert(extended.actions.end(), clearing.begin(), clearing.end());
                        keepBest(best[set | (1 << next)], std::move(extended));
                    }
                }
            }
        }
        if (best[fullSet].empty()) return giveUp("no plan");
        return verifyAndTrim(best[fullSet].front().actions);
    }

private:
    // DP의 한 후보: 현재 위치와 여기까지의 행동 열.
    struct Partial {
        Pos position;
        std::vector<Action> actions;
    };

    // 상하좌우로 붙은 블럭끼리 묶어 clusters_를 채운다 (flood fill).
    void findClusters() {
        const Grid& grid = problem_.grid;
        clusterOf_.assign(grid.rows() * grid.cols(), -1);
        for (int r = 0; r < grid.rows(); r++) {
            for (int c = 0; c < grid.cols(); c++) {
                if (grid.at(Pos{r, c}) != Cell::Block || clusterOf_[r * grid.cols() + c] >= 0) continue;
                const int id = static_cast<int>(clusters_.size());
                clusters_.emplace_back();
                std::queue<Pos> queue;
                queue.push(Pos{r, c});
                clusterOf_[r * grid.cols() + c] = id;
                while (!queue.empty()) {
                    Pos cur = queue.front();
                    queue.pop();
                    clusters_[id].push_back(cur);
                    for (const Direction& d : kDirections) {
                        Pos next = step(cur, d);
                        if (!grid.isInside(next) || grid.at(next) != Cell::Block) continue;
                        if (clusterOf_[next.row * grid.cols() + next.col] >= 0) continue;
                        clusterOf_[next.row * grid.cols() + next.col] = id;
                        queue.push(next);
                    }
                }
            }
        }
    }

    // 덩어리 집합 cleared를 치운 상태에서, from에서 출발해 덩어리 target을 치우는 짧은 해들
    // (끝나는 위치가 서로 다른 것, 최대 exitsPerState개). 부분 문제의 지도는 클래스 설명 2번 참고.
    std::vector<std::vector<Action>> clearCluster(int cleared, int target, Pos from) const {
        Problem sub = problem_;
        sub.characters[0].start = from;
        const Grid& grid = problem_.grid;
        for (int r = 0; r < grid.rows(); r++) {
            for (int c = 0; c < grid.cols(); c++) {
                const int id = clusterOf_[r * grid.cols() + c];
                if (id < 0 || id == target) continue;
                sub.grid.set(Pos{r, c}, (cleared >> id & 1) ? Cell::Empty : Cell::Obstacle);
            }
        }
        ExactSearch search(sub, kExactSearchLimits);
        return search.searchGoals(config_.exitsPerState);
    }

    // from에서 actions를 따라 움직인 뒤의 위치 (폭탄은 위치를 바꾸지 않음).
    static Pos finalPosition(Pos from, const std::vector<Action>& actions) {
        for (Action a : actions) {
            for (const Direction& d : kDirections)
                if (d.action == a) from = step(from, d);
        }
        return from;
    }

    // candidates(행동 수 오름차순)에 후보를 넣되, 같은 위치는 더 짧은 것만, 전체는 exitsPerState개까지만 남긴다.
    void keepBest(std::vector<Partial>& candidates, Partial candidate) const {
        for (auto it = candidates.begin(); it != candidates.end(); ++it) {
            if (it->position != candidate.position) continue;
            if (it->actions.size() <= candidate.actions.size()) return;
            candidates.erase(it);
            break;
        }
        auto pos = std::upper_bound(candidates.begin(), candidates.end(), candidate,
                                    [](const Partial& x, const Partial& y) { return x.actions.size() < y.actions.size(); });
        candidates.insert(pos, std::move(candidate));
        if (static_cast<int>(candidates.size()) > config_.exitsPerState) candidates.pop_back();
    }

    // 이어 붙인 행동 열을 실제 규칙으로 시뮬레이션해 합법인지 확인하고, 블럭이 모두 없어진 시점에서 자른다.
    // (클래스 설명의 "올바름"에 따라 실패할 일은 없지만, 잘못된 출력은 0점이므로 한 번 더 확인한다.)
    std::optional<std::vector<Action>> verifyAndTrim(const std::vector<Action>& actions) {
        GameState state(problem_);
        std::vector<Action> trimmed;
        for (Action a : actions) {
            if (state.remainingBlocks() == 0) break;
            trimmed.push_back(a);
            if (a == Action::Bomb) {
                state.applyBomb(0);
                continue;
            }
            for (const Direction& d : kDirections) {
                if (d.action != a) continue;
                if (!state.grid().isPassable(step(state.position(0), d))) return giveUp("illegal move");
                state.applyMove(0, d);
            }
        }
        if (state.remainingBlocks() != 0) return giveUp("blocks remain");
        return trimmed;
    }

    std::nullopt_t giveUp(const char* reason) {
        giveUpReason_ = reason;
        return std::nullopt;
    }

    const Problem& problem_;
    HierarchicalConfig config_;
    std::vector<std::vector<Pos>> clusters_;  // 덩어리 번호 → 블럭 위치들
    std::vector<int> clusterOf_;              // 칸 → 덩어리 번호 (블럭이 아니면 -1)
    std::string giveUpReason_;
};

// ---------------------------------------------------------------------------
// 풀이 선택
// ---------------------------------------------------------------------------

// 문제를 풀어 출력할 행동 목록을 만든다.
// 1. 정확한 탐색(ExactSearch)을 먼저 시도한다: 블럭 수 ≤ 16, 방문 상태 수·시간(10초) 한계 안이면 최적해.
// 2. 조건을 넘어 포기하면 빔 서치(BeamSearch)를 쓴다. 빔 서치는 greedy 해를 기준으로 시작하므로
//    결과가 greedy보다 나빠지지 않는다.
// 3. C=1이면 계층 분해(HierarchicalSolver)도 시도해, 빔 서치보다 짧으면 그 해를 쓴다.
// 환경 변수 NYPC_DEBUG가 있으면 어떤 방법을 썼는지 stderr에 적는다 (stdout 출력에는 영향 없음).
std::vector<Action> solve(const Problem& problem) {
    const bool debug = std::getenv("NYPC_DEBUG") != nullptr;
    const auto startTime = std::chrono::steady_clock::now();

    std::optional<std::vector<Action>> exact;
    std::size_t visitedStates = 0;
    std::string reason;
    {
        // 탐색이 끝나면 방문 기록(수 GB일 수 있음)을 바로 해제하도록 블록 안에 둔다.
        ExactSearch search(problem, kExactSearchLimits);
        exact = search.run();
        visitedStates = search.visitedStates();
        reason = search.giveUpReason();
    }
    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
    if (exact) {
        if (debug)
            std::cerr << "[solve] exact BFS: " << exact->size() << " actions, " << visitedStates << " states, "
                      << seconds << "s\n";
        return *exact;
    }
    if (debug)
        std::cerr << "[solve] exact BFS gave up (" << reason << ") after " << visitedStates << " states, "
                  << seconds << "s; using beam search\n";

    BeamSearch beamSearch(problem, kPlannerConfig, kBeamConfig);
    std::vector<Action> beam = beamSearch.run();
    if (debug) {
        const double total = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
        std::cerr << "[solve] greedy " << solveGreedy(problem).size() << " actions -> beam " << beam.size()
                  << " actions (" << beamSearch.distinctStates() << " beam states, " << beamSearch.duplicatesSkipped()
                  << " duplicates skipped), " << total << "s total\n";
    }

    HierarchicalSolver hierarchical(problem, kHierarchicalConfig);
    std::optional<std::vector<Action>> clustered = hierarchical.run();
    if (debug) {
        if (clustered)
            std::cerr << "[solve] hierarchical: " << clustered->size() << " actions, " << hierarchical.numClusters()
                      << " clusters\n";
        else
            std::cerr << "[solve] hierarchical gave up (" << hierarchical.giveUpReason() << ")\n";
    }
    if (clustered && clustered->size() < beam.size()) return *clustered;
    return beam;
}

// ============================================================================
// main: INPUT → PROCESS → OUTPUT
// ============================================================================

int main() {
    std::ios::sync_with_stdio(false);

    Problem problem = readProblem(std::cin);
    std::vector<Action> actions = solve(problem);
    writeActions(std::cout, actions);
    return 0;
}
