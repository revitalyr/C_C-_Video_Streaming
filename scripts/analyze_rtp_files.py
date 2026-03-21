#!/usr/bin/env python3
"""
Анализатор RTP файлов для видеостриминга
Анализирует сохраненные RTP пакеты и выводит статистику
"""

import os
import struct
import sys
import argparse
from collections import defaultdict
from datetime import datetime

class RTPAnalyzer:
    def __init__(self, filename):
        self.filename = filename
        self.packets = []
        self.stats = {
            'total_packets': 0,
            'total_bytes': 0,
            'packet_sizes': [],
            'sequence_numbers': [],
            'timestamps': [],
            'ssrc': None,
            'packet_loss': 0,
            'duplicate_packets': 0,
            'out_of_order_packets': 0,
            'first_timestamp': None,
            'last_timestamp': None
        }
        
    def analyze(self):
        """Анализ RTP файла"""
        print(f"Анализ файла: {self.filename}")
        print("=" * 50)
        
        try:
            with open(self.filename, 'rb') as f:
                packet_count = 0
                last_sequence = -1
                
                while True:
                    # Чтение RTP заголовка (минимальные 12 байт)
                    header_data = f.read(12)
                    if len(header_data) < 12:
                        break
                    
                    # Парсинг RTP заголовка
                    if len(header_data) == 12:
                        self._parse_rtp_header(header_data, packet_count, last_sequence)
                        packet_count += 1
                        last_sequence = self.stats['sequence_numbers'][-1] if self.stats['sequence_numbers'] else -1
                        
                        # Чтение полезной нагрузки
                        payload_size = 1400  # Типичный размер
                        payload = f.read(payload_size)
                        if payload:
                            self.stats['total_bytes'] += len(header_data) + len(payload)
                            self.stats['packet_sizes'].append(len(header_data) + len(payload))
                    
                    # Ограничение для больших файлов
                    if packet_count > 10000:
                        print(f"Ограничение анализа: {packet_count} пакетов")
                        break
                        
        except Exception as e:
            print(f"Ошибка при анализе файла: {e}")
            return False
            
        self._calculate_statistics()
        self._print_results()
        return True
        
    def _parse_rtp_header(self, header_data, packet_count, last_sequence):
        """Парсинг RTP заголовка"""
        # RTP заголовок структура:
        # 0                   1                   2                   3
        # 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
        # +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        # |V=2|P|X|  CC   |M|     PT      |       sequence number         |
        # +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        # |                           timestamp                           |
        # +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        # |           synchronization source (SSRC) identifier          |
        # +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        
        # Распаковка заголовка (big-endian)
        first_byte, second_byte = struct.unpack('!BB', header_data[:2])
        sequence, timestamp, ssrc = struct.unpack('!HII', header_data[2:12])
        
        # Извлечение полей
        version = (first_byte >> 6) & 0x03
        padding = (first_byte >> 5) & 0x01
        extension = (first_byte >> 4) & 0x01
        csrc_count = first_byte & 0x0F
        marker = (second_byte >> 7) & 0x01
        payload_type = second_byte & 0x7F
        
        # Сохранение статистики
        self.stats['total_packets'] += 1
        self.stats['sequence_numbers'].append(sequence)
        self.stats['timestamps'].append(timestamp)
        
        if self.stats['ssrc'] is None:
            self.stats['ssrc'] = ssrc
        elif self.stats['ssrc'] != ssrc:
            print(f"Предупреждение: разный SSRC: {self.stats['ssrc']} vs {ssrc}")
            
        if self.stats['first_timestamp'] is None:
            self.stats['first_timestamp'] = timestamp
        self.stats['last_timestamp'] = timestamp
        
        # Анализ последовательности
        if packet_count > 0:
            expected_sequence = (last_sequence + 1) % 0x10000
            if sequence == expected_sequence:
                pass  # Нормальный случай
            elif sequence == last_sequence:
                self.stats['duplicate_packets'] += 1
            elif sequence > expected_sequence:
                self.stats['packet_loss'] += sequence - expected_sequence
            else:
                # Цикл sequence number
                self.stats['out_of_order_packets'] += 1
        
    def _calculate_statistics(self):
        """Расчет дополнительной статистики"""
        if self.stats['packet_sizes']:
            self.stats['avg_packet_size'] = sum(self.stats['packet_sizes']) / len(self.stats['packet_sizes'])
            self.stats['min_packet_size'] = min(self.stats['packet_sizes'])
            self.stats['max_packet_size'] = max(self.stats['packet_sizes'])
        else:
            self.stats['avg_packet_size'] = 0
            self.stats['min_packet_size'] = 0
            self.stats['max_packet_size'] = 0
            
        # Расчет длительности
        if self.stats['timestamps'] and len(self.stats['timestamps']) > 1:
            time_diff = self.stats['last_timestamp'] - self.stats['first_timestamp']
            self.stats['duration_seconds'] = time_diff / 90000.0  # 90kHz clock
            if self.stats['duration_seconds'] > 0:
                self.stats['fps'] = self.stats['total_packets'] / self.stats['duration_seconds']
            else:
                self.stats['fps'] = 0
        else:
            self.stats['duration_seconds'] = 0
            self.stats['fps'] = 0
            
        # Расчет процента потерь
        if self.stats['total_packets'] > 0:
            total_expected = self.stats['total_packets'] + self.stats['packet_loss']
            self.stats['packet_loss_percent'] = (self.stats['packet_loss'] / total_expected) * 100
        else:
            self.stats['packet_loss_percent'] = 0
            
    def _print_results(self):
        """Вывод результатов анализа"""
        print(f"📊 Статистика файла: {self.filename}")
        print("-" * 50)
        
        # Базовая статистика
        print(f"📦 Общее количество пакетов: {self.stats['total_packets']}")
        print(f"📏 Общий размер: {self.stats['total_bytes']:,} байт")
        print(f"📈 Средний размер пакета: {self.stats['avg_packet_size']:.1f} байт")
        print(f"📊 Размер пакетов: {self.stats['min_packet_size']} - {self.stats['max_packet_size']} байт")
        
        # Временная статистика
        print(f"⏱️  Длительность: {self.stats['duration_seconds']:.2f} секунд")
        print(f"🎬 FPS: {self.stats['fps']:.2f}")
        
        # Статистика потерь
        print(f"❌ Потерянные пакеты: {self.stats['packet_loss']}")
        print(f"📉 Процент потерь: {self.stats['packet_loss_percent']:.2f}%")
        print(f"🔄 Дубликаты: {self.stats['duplicate_packets']}")
        print(f"🔀 Не по порядку: {self.stats['out_of_order_packets']}")
        
        # RTP информация
        print(f"🆔 SSRC: 0x{self.stats['ssrc']:08x}" if self.stats['ssrc'] else "🆔 SSRC: N/A")
        
        # Оценка качества
        print("\n🎯 Оценка качества:")
        quality_score = self._calculate_quality_score()
        print(f"⭐ Общая оценка: {quality_score}/100")
        
        if quality_score >= 90:
            print("✅ Отличное качество стриминга")
        elif quality_score >= 75:
            print("✅ Хорошее качество стриминга")
        elif quality_score >= 60:
            print("⚠️  Удовлетворительное качество")
        else:
            print("❌ Плохое качество стриминга")
            
        print("\n" + "=" * 50 + "\n")
        
    def _calculate_quality_score(self):
        """Расчет оценки качества стриминга"""
        score = 100
        
        # Штраф за потери пакетов
        if self.stats['packet_loss_percent'] > 0:
            score -= min(self.stats['packet_loss_percent'] * 2, 50)
            
        # Штраф за дубликаты
        if self.stats['duplicate_packets'] > 0:
            duplicate_percent = (self.stats['duplicate_packets'] / self.stats['total_packets']) * 100
            score -= min(duplicate_percent, 20)
            
        # Штраф за неупорядоченные пакеты
        if self.stats['out_of_order_packets'] > 0:
            ooo_percent = (self.stats['out_of_order_packets'] / self.stats['total_packets']) * 100
            score -= min(ooo_percent * 0.5, 10)
            
        # Бонус за стабильный FPS
        if 15 <= self.stats['fps'] <= 30:
            score += 5
        elif self.stats['fps'] > 30:
            score += 10
            
        return max(0, min(100, score))

def analyze_multiple_files(filenames):
    """Анализ нескольких файлов и сравнение"""
    print("🔍 Анализ нескольких RTP файлов")
    print("=" * 60)
    
    results = []
    
    for filename in filenames:
        if not os.path.exists(filename):
            print(f"❌ Файл не найден: {filename}")
            continue
            
        analyzer = RTPAnalyzer(filename)
        if analyzer.analyze():
            results.append({
                'filename': filename,
                'stats': analyzer.stats
            })
    
    if len(results) > 1:
        print("\n📊 Сравнительная таблица:")
        print("-" * 80)
        print(f"{'Файл':<20} {'Пакеты':<10} {'Размер(МБ)':<12} {'FPS':<8} {'Потери(%)':<10} {'Оценка':<8}")
        print("-" * 80)
        
        for result in results:
            stats = result['stats']
            size_mb = stats['total_bytes'] / (1024 * 1024)
            quality_score = RTPAnalyzer(result['filename'])._calculate_quality_score()
            
            print(f"{result['filename']:<20} "
                  f"{stats['total_packets']:<10} "
                  f"{size_mb:<12.2f} "
                  f"{stats['fps']:<8.1f} "
                  f"{stats['packet_loss_percent']:<10.2f} "
                  f"{quality_score:<8}")
        
        print("-" * 80)
        
        # Поиск лучшего результата
        best_result = max(results, key=lambda x: RTPAnalyzer(x['filename'])._calculate_quality_score())
        print(f"\n🏆 Лучший результат: {best_result['filename']}")

def main():
    parser = argparse.ArgumentParser(description='Анализатор RTP файлов')
    parser.add_argument('files', nargs='+', help='RTP файлы для анализа')
    parser.add_argument('--compare', action='store_true', help='Сравнить несколько файлов')
    
    args = parser.parse_args()
    
    if len(args.files) == 0:
        print("❌ Укажите хотя бы один RTP файл")
        sys.exit(1)
    
    print(f"🎥 RTP Анализатор - {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print("=" * 60)
    
    if args.compare or len(args.files) > 1:
        analyze_multiple_files(args.files)
    else:
        # Анализ одного файла
        filename = args.files[0]
        if not os.path.exists(filename):
            print(f"❌ Файл не найден: {filename}")
            sys.exit(1)
            
        analyzer = RTPAnalyzer(filename)
        if not analyzer.analyze():
            sys.exit(1)

if __name__ == '__main__':
    main()
