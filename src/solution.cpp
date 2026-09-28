// NYPC 2026 스텝 업 — 배찌와 다오의 대청소
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
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <queue>
#include <string>
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
    double blockWeightExponent = 1.2;  // b
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

// 캐릭터의 다음 목표 칸을 고르고 경로를 만든다.
//
// 방법:
//   1. 현재 위치에서 빈 칸만 밟는 BFS로 도달 가능한 모든 칸과 보행 거리 r, 최단 경로를 구한다.
//   2. 각 칸에서 터트렸을 때 부서질 블럭 수 x를 세고, x > 0인 칸만 후보로 삼는다.
//   3. 후보마다 점수를 매겨 가장 낮은 칸을 목표로 한다.
//        기본 점수 = r / (a·x^b) 또는 (r + 1) / (a·x^b)  (PlannerConfig::formula, a, b)
//        C=2에서 상대에게 목표가 있으면: 기본 점수 / r_z^α
//          r_z = 후보 칸과 상대 목표 칸의 맨해튼 거리.
//          의도: "상대 목표에 가까운 칸은 나쁘게" 평가해 두 캐릭터가 같은 곳으로 몰리는 것을
//          막는다. 나누는 값이 r_z 자체가 아니라 r_z^α(α = 0.5)인 이유는, r_z의 범위(0 ~ 30+)가
//          보행 거리에 비해 커서 그대로 나누면 멀리 흩어지려는 경향이 지나치게 강해지기 때문.
//          r_z = 0(상대 목표와 같은 칸)은 후보에서 제외한다.
//
// 동점 처리: BFS 방문 순서(보행 거리가 짧은 순, 같은 거리면 방향 우선순위 U, L, R, D)대로
//   후보를 보며 "더 작을 때만" 교체하므로, 동점이면 먼저 방문한 칸이 선택된다.
// 알려진 한계:
//   - r_z는 칸 사이 거리라서, K가 클 때 서로 다른 방향에서 같은 블럭을 노리는 경우를 잡지 못한다.
//   - 한 번에 하나의 목표만 보는 근시안적 선택이다 (이후의 순서는 고려하지 않음).
// 후보가 없으면 std::nullopt.
std::optional<Plan> planTarget(const GameState& state, int who, const std::optional<Pos>& partnerTarget,
                               const PlannerConfig& config) {
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
    std::optional<Pos> best;
    double bestScore = 0.0;
    for (Pos cell : visitOrder) {
        int blocks = static_cast<int>(state.blastTargetsFrom(cell, state.power(who)).size());
        if (blocks == 0) continue;

        double r = distance[cell.row][cell.col];
        double cost = (config.formula == ScoreFormula::DistancePerBlock) ? r : r + 1;
        double efficiency = config.blockWeightScale * std::pow(static_cast<double>(blocks), config.blockWeightExponent);
        double score = cost / efficiency;
        if (partnerTarget) {
            int rz = manhattan(cell, *partnerTarget);
            if (rz == 0) continue;  // 상대 목표와 같은 칸은 제외
            score /= std::pow(static_cast<double>(rz), config.repulsionExponent);
        }
        if (!best || score < bestScore - 1e-12) {
            best = cell;
            bestScore = score;
        }
    }
    if (!best) return std::nullopt;

    // 경로 복원: 목표에서 parent를 따라 거슬러 올라간 뒤 뒤집는다.
    Plan plan;
    plan.target = *best;
    for (Pos p = *best; p != start;) {
        const Direction* d = parent[p.row][p.col];
        plan.path.push_back(d);
        p = Pos{p.row - d->dRow, p.col - d->dCol};
    }
    std::reverse(plan.path.begin(), plan.path.end());
    return plan;
}

// ---------------------------------------------------------------------------
// 턴 진행
// ---------------------------------------------------------------------------

// 캐릭터들의 턴을 진행하며 행동 목록을 쌓는다. 각 캐릭터의 목표(Plan)를 보관한다.
//
// 턴 규칙:
//   1. 목표가 없으면 planTarget으로 새로 정한다 (상대의 현재 목표를 반발 기준으로 넘김).
//   2. 목표가 여전히 없으면 할 일이 없으므로 폭탄으로 턴을 버린다.
//      (C=2에서 남은 블럭이 상대 목표에만 걸린 경우 등. "제자리 대기" 행동이 없다.)
//   3. 목표 칸에 도착해 있으면 폭탄, 아니면 저장된 경로대로 한 칸 이동한다.
//   4. 폭탄으로 블럭이 하나라도 부서지면 모든 캐릭터의 목표를 지운다.
//      격자가 바뀌어 기존 점수가 더 이상 맞지 않기 때문이다 (다음 턴에 다시 BFS).
//      반대로 블럭이 부서지지 않는 동안은 격자가 그대로이므로 목표를 유지한다.
//      블럭이 부서져도 새로 막히는 칸은 없으므로, 저장한 경로가 불법 이동이 되는 일은 없다.
class TurnRunner {
public:
    TurnRunner(const Problem& problem, const PlannerConfig& config)
        : state_(problem), config_(config), plans_(problem.characters.size()) {}

    bool finished() const { return state_.remainingBlocks() == 0; }

    // 캐릭터 who의 한 턴을 결정·적용하고 그 행동을 돌려준다.
    Action playTurn(int who) {
        if (!plans_[who]) plans_[who] = planTarget(state_, who, partnerTarget(who), config_);

        if (!plans_[who] || plans_[who]->arrived()) {
            // 목표 칸에 도착했거나(유효한 폭탄) 할 일이 없음(턴 버리기)
            if (state_.applyBomb(who) > 0) clearAllPlans();
            return Action::Bomb;
        }

        Plan& plan = *plans_[who];
        const Direction* d = plan.path[plan.nextStep++];
        state_.applyMove(who, *d);
        return d->action;
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
};

// 문제를 풀어 출력할 행동 목록을 만든다.
// 배찌부터 캐릭터들이 번갈아 한 턴씩 행동하며, 블럭이 모두 없어지면 즉시 멈춘다
// (C=2에서 배찌의 턴에 끝나도 된다). 출력 길이 제한(100 000)에 도달하면 그 자리에서 멈춘다.
std::vector<Action> solve(const Problem& problem) {
    constexpr int kMaxActions = 100000;

    TurnRunner runner(problem, kPlannerConfig);
    const int numCharacters = static_cast<int>(problem.characters.size());
    std::vector<Action> actions;
    for (int turn = 0; !runner.finished() && turn < kMaxActions; turn++) {
        actions.push_back(runner.playTurn(turn % numCharacters));
    }
    return actions;
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
