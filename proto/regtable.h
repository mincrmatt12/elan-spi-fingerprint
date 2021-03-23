#pragma once

// This contains various regtables

#include <stdint.h>
#include <string.h>

namespace elan {
	struct RegEntry {
		uint8_t address, value;
	};

	struct RegTable {
		int length;
		const RegEntry *values;
	};

	template<int Length>
	struct AutoRegTable {
		operator RegTable() const {
			return data;
		}

		const RegTable& table() const {
			return data;
		}

		AutoRegTable(RegEntry *entries) {
			memcpy(values, entries, sizeof values);
			data.length = Length;
			data.values = values;
		}
	private:
		RegEntry values[Length];
		RegTable data;
	};

	template<typename ...Entries>
	auto create_reg_table(Entries ...entries) {
		const static int Length = sizeof...(Entries);
		RegEntry temp[Length] = {entries...};
		return AutoRegTable<Length>(temp);
	}

	namespace calib {
		/*
		 * 0, 5, 6, 7, default
		 *
		 *  
		 *                               05 60 06 c0 08       04 0a 97 0b 72 0c 69 0f 2b 11 2b 13 28 15 28 18 04 21 20 22 36 2a 4b
		 * 2a 07                         05 60 06 c0 07 80 08 04 0a 97 0b 72 0c 69 0f 2a 11 2a 13 27 15 67 18 04 21 20 22 36             2a 5f 2b c0       2e ff
		 * 2a 07                         05 60 06 c0 07 80 08 04 0a 97 0b 72 0c 69 0f 2a 11 2a 13 27 15 67 18 04 21 20 22 36             2a 5f 2b c0       2e ff
		 * 2a 07                         05 60 06 c0 07 80 08 04 0a 97 0b 72 0c 69 0f 2a 11 2a 13 27 15 67 18 04 21 20 22 36             2a 5f 2b c0       2e ff
		 * 2a 07 01 00 02 4f 03 00 04 4f 05 60 06 c0 07 80 08 04 0a 97 0b 72 0c 69 0f 2a 11 2a 13 27 15 67 18 04 21 20 22 36 29 02 2a 03 2a 5f 2b c0 2c 10 2e ff
		 *                               05 60 06 c0 07 80 08 04 0a 97 0b 72 0c 69 0f 2a 11 2a 13 27 15 67 18 04 21 20 22 36             2a 5f 2b c0       2e ff
		 */

		auto CalibId0 = create_reg_table(
			RegEntry{0x5,  0x60},
			RegEntry{0x6,  0xC0},
			RegEntry{0x8,  0x04},
			RegEntry{0xA,  0x97},
			RegEntry{0xB,  0x72},
			RegEntry{0xC,  0x69},
			RegEntry{0xF,  0x2B},
			RegEntry{0x11, 0x2B},
			RegEntry{0x13, 0x28},
			RegEntry{0x15, 0x28},
			RegEntry{0x18, 0x04},
			RegEntry{0x21, 0x20},
			RegEntry{0x2A, 0x4B}
		);

		auto CalibId5 = create_reg_table(
			RegEntry{0x2A, 0x07},
			
			// dac gain/calibration

			RegEntry{0x5,  0x60},
			RegEntry{0x6,  0xC0},
			
			// CTRL_REG_DAC_2?

			RegEntry{0x7,  0x80},

			RegEntry{0x8,  0x04},
			RegEntry{0xA,  0x97},
			RegEntry{0xB,  0x72},
			RegEntry{0xC,  0x69},
			RegEntry{0xF,  0x2A},
			RegEntry{0x11, 0x2A},
			RegEntry{0x13, 0x27},
			RegEntry{0x15, 0x67},
			RegEntry{0x18, 0x04},
			RegEntry{0x21, 0x20},
			RegEntry{0x22, 0x36},
			// apparently a mode register of some kind
			RegEntry{0x2A, 0x5F},
			RegEntry{0x2B, 0xC0},
			RegEntry{0x2E, 0xFF}
		);

		// Identical to CalibId5
		auto CalibId6 = create_reg_table(
			RegEntry{0x2A, 0x07},
			
			// dac gain/calibration

			RegEntry{0x5,  0x60},
			RegEntry{0x6,  0xC0},
			
			// CTRL_REG_DAC_2?

			RegEntry{0x7,  0x80},

			RegEntry{0x8,  0x04},
			RegEntry{0xA,  0x97},
			RegEntry{0xB,  0x72},
			RegEntry{0xC,  0x69},
			RegEntry{0xF,  0x2A},
			RegEntry{0x11, 0x2A},
			RegEntry{0x13, 0x27},
			RegEntry{0x15, 0x67},
			RegEntry{0x18, 0x04},
			RegEntry{0x21, 0x20},
			RegEntry{0x22, 0x36},
			RegEntry{0x2A, 0x5F},
			RegEntry{0x2B, 0xC0},
			RegEntry{0x2E, 0xFF}
		);

		// Identical to CalibId5
		auto CalibId7 = create_reg_table(
			RegEntry{0x2A, 0x07},
			
			// dac gain/calibration

			RegEntry{0x5,  0x60},
			RegEntry{0x6,  0xC0},
			
			// CTRL_REG_DAC_2?

			RegEntry{0x7,  0x80},

			RegEntry{0x8,  0x04},
			RegEntry{0xA,  0x97},
			RegEntry{0xB,  0x72},
			RegEntry{0xC,  0x69},
			RegEntry{0xF,  0x2A},
			RegEntry{0x11, 0x2A},
			RegEntry{0x13, 0x27},
			RegEntry{0x15, 0x67},
			RegEntry{0x18, 0x04},
			RegEntry{0x21, 0x20},
			RegEntry{0x22, 0x36},
			RegEntry{0x2A, 0x5F},
			RegEntry{0x2B, 0xC0},
			RegEntry{0x2E, 0xFF}
		);

		auto CalibId12 = create_reg_table(
			RegEntry{0x2a, 0x07},
			RegEntry{0x01, 0x00},
			RegEntry{0x02, 0x4f},
			RegEntry{0x03, 0x00},
			RegEntry{0x04, 0x4f},
			RegEntry{0x05, 0x60},
			RegEntry{0x06, 0xc0},
			RegEntry{0x07, 0x80},
			RegEntry{0x08, 0x04},
			RegEntry{0x0a, 0x97},
			RegEntry{0x0b, 0x72},
			RegEntry{0x0c, 0x69},
			RegEntry{0x0f, 0x2a},
			RegEntry{0x11, 0x2a},
			RegEntry{0x13, 0x27},
			RegEntry{0x15, 0x67},
			RegEntry{0x18, 0x04},
			RegEntry{0x21, 0x20},
			RegEntry{0x22, 0x36},
			RegEntry{0x29, 0x02},
			RegEntry{0x2a, 0x03},
			RegEntry{0x2a, 0x5f},
			RegEntry{0x2b, 0xc0},
			RegEntry{0x2c, 0x10},
			RegEntry{0x2e, 0xff}
		);

		auto CalibIdDefault = create_reg_table(
			RegEntry{0x05, 0x60},
			RegEntry{0x06, 0xc0},
			RegEntry{0x07, 0x80},
			RegEntry{0x08, 0x04},
			RegEntry{0x0a, 0x97},
			RegEntry{0x0b, 0x72},
			RegEntry{0x0c, 0x69},
			RegEntry{0x0f, 0x2a},
			RegEntry{0x11, 0x2a},
			RegEntry{0x13, 0x27},
			RegEntry{0x15, 0x67},
			RegEntry{0x18, 0x04},
			RegEntry{0x21, 0x20},
			RegEntry{0x22, 0x36},
			RegEntry{0x2a, 0x5f},
			RegEntry{0x2b, 0xc0},
			RegEntry{0x2e, 0xff}
		);

	};
}
