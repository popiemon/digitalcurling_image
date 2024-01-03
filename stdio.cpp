#include <cassert>
#include <cstdlib>
#include <stdexcept>
#include <iostream>
#include <thread>
#include <fstream>
#include <boost/asio.hpp>
#include "digitalcurling3/digitalcurling3.hpp"

namespace dc = digitalcurling3;

namespace {



dc::Team g_team;
dc::GameSetting g_game_setting;
std::unique_ptr<dc::ISimulator> g_simulator;
std::unique_ptr<dc::ISimulatorStorage> g_simulator_storage;
std::array<std::unique_ptr<dc::IPlayer>, 4> g_players;
/// \brief 試合設定に対して試合前の準備を行う．
///
/// この処理中の思考時間消費はありません．試合前に時間のかかる処理を行う場合この中で行うべきです．
///
/// \param team この思考エンジンのチームID．
///     Team::k0 の場合，最初のエンドの先攻です．
///     Team::k1 の場合，最初のエンドの後攻です．
///
/// \param game_setting 試合設定．
///     この参照はOnInitの呼出し後は無効になります．OnInitの呼出し後にも参照したい場合はコピーを作成してください．
///
/// \param simulator_factory 試合で使用されるシミュレータの情報．
///     未対応のシミュレータの場合 nullptr が格納されます．
///
/// \param player_factories 自チームのプレイヤー情報．
///     未対応のプレイヤーの場合 nullptr が格納されます．
///
/// \param player_order 出力用引数．
///     プレイヤーの順番(デフォルトで0, 1, 2, 3)を変更したい場合は変更してください．
void OnInit(
    dc::Team team,
    dc::GameSetting const& game_setting,
    std::unique_ptr<dc::ISimulatorFactory> simulator_factory,
    std::array<std::unique_ptr<dc::IPlayerFactory>, 4> player_factories,
    std::array<size_t, 4> & player_order)
{
    if (simulator_factory == nullptr || simulator_factory->GetSimulatorId() != "fcv1") {
        std::cout << "warning!: Unsupported simulator!"
            " EstimateShotVelocityFCV1() is only available for \"fcv1\" simulator." << std::endl;
    }

    // 非対応の場合は シミュレータFCV1を使用する.
    g_team = team;
    g_game_setting = game_setting;
    if (simulator_factory) {
        g_simulator = simulator_factory->CreateSimulator();
    } else {
        g_simulator = dc::simulators::SimulatorFCV1Factory().CreateSimulator();
    }
    g_simulator_storage = g_simulator->CreateStorage();

    // プレイヤーを生成する
    // 非対応の場合は NormalDistプレイヤーを使用する.
    assert(g_players.size() == player_factories.size());
    for (size_t i = 0; i < g_players.size(); ++i) {
        auto const& player_factory = player_factories[player_order[i]];
        if (player_factory) { 
            g_players[i] = player_factory->CreatePlayer();
        } else {
            g_players[i] = dc::players::PlayerNormalDistFactory().CreatePlayer();
        }
    }
}

void to_json_gamestate(nlohmann::json& j, dc::GameState const& v)
{
    j["end"] = v.end;
    j["shot"] = v.shot;
    j["hammer"] = v.hammer;
    j["stones"]["team0"] = v.stones[0];
    j["stones"]["team1"] = v.stones[1];
    j["scores"]["team0"] = v.scores[0];
    j["scores"]["team1"] = v.scores[1];
    j["extra_end_score"]["team0"] = v.extra_end_score[0];
    j["extra_end_score"]["team1"] = v.extra_end_score[1];
    j["thinking_time_remaining"]["team0"] = v.thinking_time_remaining[0];
    j["thinking_time_remaining"]["team1"] = v.thinking_time_remaining[1];
    j["game_result"] = v.game_result;
}

void from_json_gamestate(nlohmann::json const& j, dc::GameState& v)
{
    j.at("end").get_to(v.end);
    j.at("shot").get_to(v.shot);
    j.at("hammer").get_to(v.hammer);
    j.at("stones").at("team0").get_to(v.stones[0]);
    j.at("stones").at("team1").get_to(v.stones[1]);
    j.at("scores").at("team0").get_to(v.scores[0]);
    j.at("scores").at("team1").get_to(v.scores[1]);
    j.at("extra_end_score").at("team0").get_to(v.extra_end_score[0]);
    j.at("extra_end_score").at("team1").get_to(v.extra_end_score[1]);
    j.at("thinking_time_remaining").at("team0").get_to(v.thinking_time_remaining[0]);
    j.at("thinking_time_remaining").at("team1").get_to(v.thinking_time_remaining[1]);
    j.at("game_result").get_to(v.game_result);
}

/// \brief 自チームのターンに呼ばれます．行動を選択し，返してください．
///
/// \param game_state 現在の試合状況．
///     この参照は関数の呼出し後に無効になりますので，関数呼出し後に参照したい場合はコピーを作成してください．
///
/// \return 選択する行動．この行動が自チームの行動としてサーバーに送信される．
dc::Move OnMyTurn(dc::GameState const& game_state)
{
    nlohmann::json j;
    dc::GameState temp_game_state;
    dc::Move move;
    temp_game_state = game_state;
    std::cout << "start_my_turn" << temp_game_state.shot << std::endl;
    // std::array<StoneIndex, 16> sorted_indices;
    g_simulator->Save(*g_simulator_storage);
    // このサンプルでは標準入力を受け取ってMoveに変換しています．
    while (true) {
        g_simulator->Load(*g_simulator_storage);
        temp_game_state = game_state;
        to_json_gamestate(j, temp_game_state);
        auto & current_player = *g_players[temp_game_state.shot / 4];
        std::string line;
        std::cout << "inputwait " << j << std::endl;
        if (!std::getline(std::cin, line)) {
            throw std::runtime_error("std::getline");
        }
        dc::moves::Shot shot;
        std::string shot_rotation_str;
        std::string shot_class;
        std::istringstream line_buf(line);
        std::string input_game_state;
        line_buf >> shot.velocity.x >> shot.velocity.y >> shot_rotation_str >> shot_class >> input_game_state;
        std::cout << "inputget" << std::endl;
        if (shot_rotation_str == "cw") {
            shot.rotation = dc::moves::Shot::Rotation::kCW;
        } else if (shot_rotation_str == "ccw") {
            shot.rotation = dc::moves::Shot::Rotation::kCCW; 
        } else {
            continue;
        }
        if (shot_class == "shot")
        {
            move = shot;
            break;
        }
        else
        {
            if (shot_class == "simufile") {
                nlohmann::json json_obj = nlohmann::json::parse(input_game_state);
                from_json_gamestate(input_game_state, temp_game_state);
            }
            else {
                temp_game_state = game_state;
            }
            dc::Move temp_move = shot;
            auto & current_player = *g_players[temp_game_state.shot / 4];
            dc::ApplyMove(g_game_setting, *g_simulator, current_player, temp_game_state, temp_move, std::chrono::milliseconds(0));
            to_json_gamestate(j, temp_game_state);
            std::cout << "jsonoutput " << j <<std::endl;
            continue;
        }
    }
    std::cout << "end_my_turn" << temp_game_state.shot << std::endl;
    return move;
}



/// \brief 相手チームのターンに呼ばれます．AIを作る際にこの関数の中身を記述する必要は無いかもしれません．
///
/// \param game_state 現在の試合状況．
///     この参照は関数の呼出し後に無効になりますので，関数呼出し後に参照したい場合はコピーを作成してください．
void OnOpponentTurn(dc::GameState const& game_state)
{
    // やることは無いです
}



/// \brief ゲームが正常に終了した際にはこの関数が呼ばれます．
///
/// \param game_state 現在の試合状況．
///     この参照は関数の呼出し後に無効になりますので，関数呼出し後に参照したい場合はコピーを作成してください．
void OnGameOver(dc::GameState const& game_state)
{
    if (game_state.game_result->winner == g_team) {
        std::cout << "won the game" << std::endl;
    } else {
        std::cout << "lost the game" << std::endl;
    }
}



} // unnamed namespace



int main(int argc, char const * argv[])
{
    using boost::asio::ip::tcp;
    using nlohmann::json;

    // TODO AIの名前を変更する場合はここを変更してください．
    constexpr auto kName = "stdio";

    constexpr int kSupportedProtocolVersionMajor = 1;

    try {
        if (argc != 3) {
            std::cerr << "Usage: command <host> <port>" << std::endl;
            return 1;
        }

        boost::asio::io_context io_context;

        tcp::socket socket(io_context);
        tcp::resolver resolver(io_context);
        boost::asio::connect(socket, resolver.resolve(argv[1], argv[2]));  // 引数のホスト，ポートに接続します．

        // ソケットから1行読む関数です．バッファが空の場合，新しい行が来るまでスレッドをブロックします．
        auto read_next_line = [&socket, input_buffer = std::string()] () mutable {
            // read_untilの結果，input_bufferに複数行入ることがあるため，1行ずつ取り出す処理を行っている
            if (input_buffer.empty()) {
                boost::asio::read_until(socket, boost::asio::dynamic_buffer(input_buffer), '\n');
            }
            auto new_line_pos = input_buffer.find_first_of('\n');
            auto line = input_buffer.substr(0, new_line_pos + 1);
            input_buffer.erase(0, new_line_pos + 1);
            return line;
        };

        // コマンドが予期したものかチェックする関数です．
        auto check_command = [] (nlohmann::json const& jin, std::string_view expected_cmd) {
            auto const actual_cmd = jin.at("cmd").get<std::string>();
            if (actual_cmd != expected_cmd) {
                std::ostringstream buf;
                buf << "Unexpected cmd (expected: \"" << expected_cmd << "\", actual: \"" << actual_cmd << "\")";
                throw std::runtime_error(buf.str());
            }
        };

        dc::Team team = dc::Team::kInvalid;

        // [in] dc
        {
            auto const line = read_next_line();
            auto const jin = json::parse(line);

            check_command(jin, "dc");

            auto const& jin_version = jin.at("version");
            if (jin_version.at("major").get<int>() != kSupportedProtocolVersionMajor) {
                throw std::runtime_error("Unexpected protocol version");
            }

            std::cout << "[in] dc" << std::endl;
            std::cout << "  game_id  : " << jin.at("game_id").get<std::string>() << std::endl;
            std::cout << "  date_time: " << jin.at("date_time").get<std::string>() << std::endl;
        }

        // [out] dc_ok
        {
            json const jout = {
                { "cmd", "dc_ok" },
                { "name", kName }
            };
            auto const output_message = jout.dump() + '\n';
            boost::asio::write(socket, boost::asio::buffer(output_message));

            std::cout << "[out] dc_ok" << std::endl;
            std::cout << "  name: " << kName << std::endl;
        }


        // [in] is_ready
        {
            auto const line = read_next_line();
            auto const jin = json::parse(line);

            check_command(jin, "is_ready");

            if (jin.at("game").at("rule").get<std::string>() != "normal") {
                throw std::runtime_error("Unexpected rule");
            }

            team = jin.at("team").get<dc::Team>();

            auto const game_setting = jin.at("game").at("setting").get<dc::GameSetting>();

            auto const& jin_simulator = jin.at("game").at("simulator");
            std::unique_ptr<dc::ISimulatorFactory> simulator_factory;
            try {
                simulator_factory = jin_simulator.get<std::unique_ptr<dc::ISimulatorFactory>>();
            } catch (std::exception & e) {
                std::cout << "Exception: " << e.what() << std::endl;
            }

            auto const& jin_player_factories = jin.at("game").at("players").at(dc::ToString(team));
            std::array<std::unique_ptr<dc::IPlayerFactory>, 4> player_factories;
            for (size_t i = 0; i < 4; ++i) {
                std::unique_ptr<dc::IPlayerFactory> player_factory;
                try {
                    player_factory = jin_player_factories[i].get<std::unique_ptr<dc::IPlayerFactory>>();
                } catch (std::exception & e) {
                    std::cout << "Exception: " << e.what() << std::endl;
                }
                player_factories[i] = std::move(player_factory);
            }

            std::cout << "[in] is_ready" << std::endl;
        
        // [out] ready_ok

            std::array<size_t, 4> player_order{ 0, 1, 2, 3 };
            OnInit(team, game_setting, std::move(simulator_factory), std::move(player_factories), player_order);

            json const jout = {
                { "cmd", "ready_ok" },
                { "player_order", player_order }
            };
            auto const output_message = jout.dump() + '\n';
            boost::asio::write(socket, boost::asio::buffer(output_message));

            std::cout << "[out] ready_ok" << std::endl;
            std::cout << "  player order: " << jout.at("player_order").dump() << std::endl;
        }

        // [in] new_game
        {
            auto const line = read_next_line();
            auto const jin = json::parse(line);

            check_command(jin, "new_game");

            std::cout << "[in] new_game" << std::endl;
            std::cout << "  team 0: " << jin.at("name").at("team0") << std::endl;
            std::cout << "  team 1: " << jin.at("name").at("team1") << std::endl;
        }

        dc::GameState game_state;

        while (true) {
            // [in] update
            auto const line = read_next_line();
            auto const jin = json::parse(line);

            check_command(jin, "update");

            game_state = jin.at("state").get<dc::GameState>();

            std::cout << "[in] update (end: " << int(game_state.end) << ", shot: " << int(game_state.shot) << ")" << std::endl;
            // if game was over
            if (game_state.game_result) {
                break;
            }

            if (game_state.GetNextTeam() == team) { // my turn
                // [out] move
                auto move = OnMyTurn(game_state);
                json jout = {
                    { "cmd", "move" },
                    { "move", move }
                };
                auto const output_message = jout.dump() + '\n';
                boost::asio::write(socket, boost::asio::buffer(output_message));
                
                std::cout << "[out] move" << std::endl;
                if (std::holds_alternative<dc::moves::Shot>(move)) {
                    dc::moves::Shot const& shot = std::get<dc::moves::Shot>(move);
                    std::cout << "  type    : shot" << std::endl;
                    std::cout << "  velocity: [" << shot.velocity.x << ", " << shot.velocity.y << "]" << std::endl;
                    std::cout << "  rotation: " << (shot.rotation == dc::moves::Shot::Rotation::kCCW ? "ccw" : "cw") << std::endl;
                } else if (std::holds_alternative<dc::moves::Concede>(move)) {
                    std::cout << "  type: concede" << std::endl;
                }

            } else { // opponent turn
                OnOpponentTurn(game_state);
            }
        }

        // [in] game_over
        {
            auto const line = read_next_line();
            auto const jin = json::parse(line);

            check_command(jin, "game_over");

            std::cout << "[in] game_over" << std::endl;
        }

        // 終了．
        OnGameOver(game_state);

    } catch (std::exception & e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception" << std::endl;
    }

    return 0;
}