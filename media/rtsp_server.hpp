#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include <functional>
#include <vector>
#include <map>

import video_streaming.logger;
#include "../common/types.hpp"

#include "frame.hpp"
#include "synthetic_encoder.hpp"
#include "ffmpeg_h264_encoder.hpp"

namespace video_streaming {

/**
 * @brief Локальный RTSP сервер для стриминга видео
 * 
 * Реализует базовый RTSP протокол для стриминга H.264 видео
 * с использованием локальных видеофайлов или синтетических данных
 */
class RTSPServer {
public:
    /**
     * @brief Конфигурация RTSP сервера
     */
    struct Config {
        std::string bind_address = "127.0.0.1";  ///< Адрес для привязки
        uint16_t rtsp_port = 8554;              ///< RTSP порт
        uint16_t rtp_port_start = 5000;          ///< Начальный порт для RTP
        std::string video_file = "";             ///< Путь к видеофайлу (пусто = синтетический)
        bool enable_audio = false;               ///< Включить аудио
        uint32_t fps = 25;                       ///< Частота кадров
        uint32_t bitrate = 2000000;             ///< Битрейт (bps)
        uint32_t gop_size = 30;                 ///< Размер GOP
        bool enable_logging = true;              ///< Включить логирование
    };

    /**
     * @brief Статистика сервера
     */
    struct Stats {
        uint64_t total_connections = 0;          ///< Всего подключений
        uint64_t active_connections = 0;         ///< Активных подключений
        uint64_t frames_sent = 0;                ///< Отправлено кадров
        uint64_t bytes_sent = 0;                 ///< Отправлено байт
        uint64_t rtp_packets_sent = 0;           ///< Отправлено RTP пакетов
        double current_fps = 0.0;                ///< Текущий FPS
        std::chrono::steady_clock::time_point start_time; ///< Время запуска
    };

    /**
     * @brief Callback для обработки RTSP запросов
     */
    using RequestHandler = std::function<std::string(const std::string& method, 
                                                     const std::string& url, 
                                                     const std::map<std::string, std::string>& headers)>;

public:
    /**
     * @brief Конструктор
     * @param config Конфигурация сервера
     */
    explicit RTSPServer(const Config& config);

    /**
     * @brief Деструктор
     */
    ~RTSPServer();

    /**
     * @brief Запуск сервера
     * @return true если сервер запущен успешно
     */
    bool start();

    /**
     * @brief Остановка сервера
     */
    void stop();

    /**
     * @brief Проверка запущен ли сервер
     * @return true если сервер запущен
     */
    bool is_running() const { return m_running.load(); }

    /**
     * @brief Получение конфигурации
     * @return Конфигурация сервера
     */
    const Config& get_config() const { return m_config; }

    /**
     * @brief Получение статистики
     * @return Статистика сервера
     */
    Stats get_stats() const;

    /**
     * @brief Установка обработчика запросов
     * @param handler Функция-обработчик
     */
    void set_request_handler(RequestHandler handler);

    /**
     * @brief Получение RTSP URL
     * @return RTSP URL сервера
     */
    std::string get_rtsp_url() const;

    /**
     * @brief Отправка кадра всем клиентам
     * @param frame_data Данные кадра
     * @param frame_size Размер кадра
     * @param timestamp Временная метка
     */
    void send_frame(const uint8_t* frame_data, size_t frame_size, uint32_t timestamp);

private:
    /**
     * @brief Основной поток сервера
     */
    void server_loop();

    /**
     * @brief Поток стриминга видео
     */
    void streaming_loop();

    /**
     * @brief Обработка RTSP запроса
     * @param client_socket Сокет клиента
     */
    void handle_client(int client_socket);

    /**
     * @brief Обработка OPTIONS запроса
     * @return RTSP ответ
     */
    std::string handle_options();

    /**
     * @brief Обработка DESCRIBE запроса
     * @return RTSP ответ с SDP
     */
    std::string handle_describe();

    /**
     * @brief Обработка SETUP запроса
     * @param headers Заголовки запроса
     * @return RTSP ответ
     */
    std::string handle_setup(const std::map<std::string, std::string>& headers);

    /**
     * @brief Обработка PLAY запроса
     * @return RTSP ответ
     */
    std::string handle_play();

    /**
     * @brief Обработка TEARDOWN запроса
     * @return RTSP ответ
     */
    std::string handle_teardown();

    /**
     * @brief Генерация SDP описания
     * @return SDP строка
     */
    std::string generate_sdp();

    /**
     * @brief Создание RTP пакета
     * @param payload Полезная нагрузка
     * @param payload_size Размер полезной нагрузки
     * @param sequence Sequence number
     * @param timestamp Timestamp
     * @param ssrc SSRC
     * @return RTP пакет
     */
    std::vector<uint8_t> create_rtp_packet(const uint8_t* payload, size_t payload_size,
                                          uint16_t sequence, uint32_t timestamp, uint32_t ssrc);

    /**
     * @brief Отправка RTP пакета
     * @param packet RTP пакет
     * @param client_socket Сокет клиента
     */
    void send_rtp_packet(const std::vector<uint8_t>& packet, int client_socket);

    /**
     * @brief Инициализация видео источника
     * @return true если инициализация успешна
     */
    bool init_video_source();

    /**
     * @brief Получение следующего кадра
     * @return Кадр или nullptr если конец
     */
    std::unique_ptr<VideoFrame> get_next_frame();

    /**
     * @brief Логирование сообщения
     * @param message Сообщение
     */
    void log_info(const std::string& message);

    /**
     * @brief Логирование ошибки
     * @param message Сообщение
     */
    void log_error(const std::string& message);

private:
    Config m_config;                                    ///< Конфигурация
    std::atomic<bool> m_running{false};                ///< Флаг работы
    std::thread m_server_thread;                        ///< Поток сервера
    std::thread m_streaming_thread;                     ///< Поток стриминга
    int m_server_socket = -1;                          ///< Серверный сокет
    std::vector<int> m_client_sockets;                  ///< Сокеты клиентов
    mutable std::mutex m_clients_mutex;                 ///< Мьютекс для клиентов
    RequestHandler m_request_handler;                  ///< Обработчик запросов
    Stats m_stats;                                     ///< Статистика
    mutable std::mutex m_stats_mutex;                   ///< Мьютекс для статистики
    
    // Видео источник
    std::unique_ptr<VideoEncoder> m_encoder;           ///< Видео кодировщик
    std::unique_ptr<VideoFrame> m_current_frame;        ///< Текущий кадр
    std::atomic<uint32_t> m_frame_count{0};             ///< Счетчик кадров
    std::atomic<uint16_t> m_rtp_sequence{0};            ///< RTP sequence
    std::atomic<uint32_t> m_rtp_timestamp{0};           ///< RTP timestamp
    const uint32_t m_rtp_ssrc = 12345;                  ///< RTP SSRC
    
    // Логирование
    std::shared_ptr<Logger> m_logger;                   ///< Логгер
};

} // namespace video_streaming
