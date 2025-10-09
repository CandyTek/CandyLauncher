#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// 单位类型枚举
enum class QuantityType {
	Acceleration,
	Angle,
	Area,
	Duration,
	Energy,
	Information,
	Length,
	Mass,
	Power,
	Pressure,
	Speed,
	Temperature,
	Volume
};

// 单位信息结构
struct UnitInfo {
	std::string name;
	std::string pluralName;
	std::string abbreviation;
	double conversionFactor; // 转换为基础单位的系数
	std::function<double(double)> toBase; // 转换到基础单位的函数（用于温度等非线性转换）
	std::function<double(double)> fromBase; // 从基础单位转换的函数

	UnitInfo(const std::string& n, const std::string& pn, const std::string& abbr, double factor)
		: name(n), pluralName(pn), abbreviation(abbr), conversionFactor(factor),
		toBase([factor](double v) { return v * factor; }),
		fromBase([factor](double v) { return v / factor; }) {}

	UnitInfo(const std::string& n, const std::string& pn, const std::string& abbr,
		std::function<double(double)> to, std::function<double(double)> from)
		: name(n), pluralName(pn), abbreviation(abbr), conversionFactor(1.0),
		toBase(to), fromBase(from) {}
};

// 单位量信息
struct QuantityInfo {
	QuantityType type;
	std::string name;
	std::string baseUnit;
	std::vector<UnitInfo> units;

	QuantityInfo(QuantityType t, const std::string& n, const std::string& bu)
		: type(t), name(n), baseUnit(bu) {}
};

// 单位转换数据库
class UnitConversionDatabase {
public:
	static UnitConversionDatabase& GetInstance() {
		static UnitConversionDatabase instance;
		return instance;
	}

	const std::vector<QuantityInfo>& GetAllQuantities() const {
		return quantities;
	}

	const QuantityInfo* FindQuantityByUnits(const std::string& fromUnit, const std::string& toUnit) const {
		for (const auto& quantity : quantities) {
			bool hasFrom = false, hasTo = false;
			for (const auto& unit : quantity.units) {
				if (MatchesUnit(unit, fromUnit)) hasFrom = true;
				if (MatchesUnit(unit, toUnit)) hasTo = true;
			}
			if (hasFrom && hasTo) return &quantity;
		}
		return nullptr;
	}

	const UnitInfo* FindUnit(const QuantityInfo& quantity, const std::string& unitName) const {
		for (const auto& unit : quantity.units) {
			if (MatchesUnit(unit, unitName)) {
				return &unit;
			}
		}
		return nullptr;
	}

private:
	std::vector<QuantityInfo> quantities;

	static bool MatchesUnit(const UnitInfo& unit, const std::string& name) {
		return strcasecmp(unit.name, name) ||
			strcasecmp(unit.pluralName, name) ||
			strcasecmp(unit.abbreviation, name);
	}

	static bool strcasecmp(const std::string& a, const std::string& b) {
		if (a.length() != b.length()) return false;
		for (size_t i = 0; i < a.length(); ++i) {
			if (std::tolower(a[i]) != std::tolower(b[i])) return false;
		}
		return true;
	}

	UnitConversionDatabase() {
		InitializeUnits();
	}

	void InitializeUnits() {
		// 长度 (基础单位: 米)
		QuantityInfo length(QuantityType::Length, "Length", "Meter");
		length.units.emplace_back("Meter", "Meters", "m", 1.0);
		length.units.emplace_back("Kilometer", "Kilometers", "km", 1000.0);
		length.units.emplace_back("Centimeter", "Centimeters", "cm", 0.01);
		length.units.emplace_back("Millimeter", "Millimeters", "mm", 0.001);
		length.units.emplace_back("Micrometer", "Micrometers", "μm", 0.000001);
		length.units.emplace_back("Nanometer", "Nanometers", "nm", 0.000000001);
		length.units.emplace_back("Mile", "Miles", "mi", 1609.344);
		length.units.emplace_back("Yard", "Yards", "yd", 0.9144);
		length.units.emplace_back("Foot", "Feet", "ft", 0.3048);
		length.units.emplace_back("Inch", "Inches", "in", 0.0254);
		length.units.emplace_back("NauticalMile", "NauticalMiles", "nmi", 1852.0);
		quantities.push_back(length);

		// 质量 (基础单位: 千克)
		QuantityInfo mass(QuantityType::Mass, "Mass", "Kilogram");
		mass.units.emplace_back("Kilogram", "Kilograms", "kg", 1.0);
		mass.units.emplace_back("Gram", "Grams", "g", 0.001);
		mass.units.emplace_back("Milligram", "Milligrams", "mg", 0.000001);
		mass.units.emplace_back("Tonne", "Tonnes", "t", 1000.0);
		mass.units.emplace_back("Pound", "Pounds", "lb", 0.45359237);
		mass.units.emplace_back("Ounce", "Ounces", "oz", 0.028349523125);
		mass.units.emplace_back("Stone", "Stones", "st", 6.35029318);
		mass.units.emplace_back("UsOunce", "UsOunces", "oz", 0.028349523125);
		mass.units.emplace_back("ImperialOunce", "ImperialOunces", "oz", 0.028349523125);
		quantities.push_back(mass);

		// 温度 (基础单位: 开尔文)
		QuantityInfo temperature(QuantityType::Temperature, "Temperature", "Kelvin");
		temperature.units.emplace_back("Kelvin", "Kelvins", "K", 1.0);
		temperature.units.emplace_back("DegreeCelsius", "DegreesCelsius", "°C",
			[](double c) { return c + 273.15; },
			[](double k) { return k - 273.15; });
		temperature.units.emplace_back("DegreeFahrenheit", "DegreesFahrenheit", "°F",
			[](double f) { return (f - 32.0) * 5.0 / 9.0 + 273.15; },
			[](double k) { return (k - 273.15) * 9.0 / 5.0 + 32.0; });
		quantities.push_back(temperature);

		// 面积 (基础单位: 平方米)
		QuantityInfo area(QuantityType::Area, "Area", "SquareMeter");
		area.units.emplace_back("SquareMeter", "SquareMeters", "m²", 1.0);
		area.units.emplace_back("SquareKilometer", "SquareKilometers", "km²", 1000000.0);
		area.units.emplace_back("SquareCentimeter", "SquareCentimeters", "cm²", 0.0001);
		area.units.emplace_back("SquareMillimeter", "SquareMillimeters", "mm²", 0.000001);
		area.units.emplace_back("SquareMile", "SquareMiles", "mi²", 2589988.110336);
		area.units.emplace_back("SquareYard", "SquareYards", "yd²", 0.83612736);
		area.units.emplace_back("SquareFoot", "SquareFeet", "ft²", 0.09290304);
		area.units.emplace_back("SquareInch", "SquareInches", "in²", 0.00064516);
		area.units.emplace_back("Hectare", "Hectares", "ha", 10000.0);
		area.units.emplace_back("Acre", "Acres", "ac", 4046.8564224);
		quantities.push_back(area);

		// 体积 (基础单位: 立方米)
		QuantityInfo volume(QuantityType::Volume, "Volume", "CubicMeter");
		volume.units.emplace_back("CubicMeter", "CubicMeters", "m³", 1.0);
		volume.units.emplace_back("Liter", "Liters", "L", 0.001);
		volume.units.emplace_back("Milliliter", "Milliliters", "mL", 0.000001);
		volume.units.emplace_back("CubicCentimeter", "CubicCentimeters", "cm³", 0.000001);
		volume.units.emplace_back("CubicFoot", "CubicFeet", "ft³", 0.028316846592);
		volume.units.emplace_back("CubicInch", "CubicInches", "in³", 0.000016387064);
		volume.units.emplace_back("UsGallon", "UsGallons", "gal", 0.003785411784);
		volume.units.emplace_back("ImperialGallon", "ImperialGallons", "gal", 0.00454609);
		volume.units.emplace_back("Quart", "Quarts", "qt", 0.000946352946);
		volume.units.emplace_back("Pint", "Pints", "pt", 0.000473176473);
		quantities.push_back(volume);

		// 速度 (基础单位: 米每秒)
		QuantityInfo speed(QuantityType::Speed, "Speed", "MeterPerSecond");
		speed.units.emplace_back("MeterPerSecond", "MetersPerSecond", "m/s", 1.0);
		speed.units.emplace_back("KilometerPerHour", "KilometersPerHour", "km/h", 0.277777778);
		speed.units.emplace_back("MilePerHour", "MilesPerHour", "mph", 0.44704);
		speed.units.emplace_back("FootPerSecond", "FeetPerSecond", "ft/s", 0.3048);
		speed.units.emplace_back("Knot", "Knots", "kn", 0.514444444);
		quantities.push_back(speed);

		// 时间 (基础单位: 秒)
		QuantityInfo duration(QuantityType::Duration, "Duration", "Second");
		duration.units.emplace_back("Second", "Seconds", "s", 1.0);
		duration.units.emplace_back("Minute", "Minutes", "min", 60.0);
		duration.units.emplace_back("Hour", "Hours", "h", 3600.0);
		duration.units.emplace_back("Day", "Days", "d", 86400.0);
		duration.units.emplace_back("Week", "Weeks", "wk", 604800.0);
		duration.units.emplace_back("Month", "Months", "mo", 2628000.0);
		duration.units.emplace_back("Year", "Years", "yr", 31536000.0);
		duration.units.emplace_back("Millisecond", "Milliseconds", "ms", 0.001);
		duration.units.emplace_back("Microsecond", "Microseconds", "μs", 0.000001);
		duration.units.emplace_back("Nanosecond", "Nanoseconds", "ns", 0.000000001);
		quantities.push_back(duration);

		// 能量 (基础单位: 焦耳)
		QuantityInfo energy(QuantityType::Energy, "Energy", "Joule");
		energy.units.emplace_back("Joule", "Joules", "J", 1.0);
		energy.units.emplace_back("Kilojoule", "Kilojoules", "kJ", 1000.0);
		energy.units.emplace_back("Calorie", "Calories", "cal", 4.184);
		energy.units.emplace_back("Kilocalorie", "Kilocalories", "kcal", 4184.0);
		energy.units.emplace_back("WattHour", "WattHours", "Wh", 3600.0);
		energy.units.emplace_back("KilowattHour", "KilowattHours", "kWh", 3600000.0);
		energy.units.emplace_back("ElectronVolt", "ElectronVolts", "eV", 1.602176634e-19);
		energy.units.emplace_back("BritishThermalUnit", "BritishThermalUnits", "BTU", 1055.05585);
		quantities.push_back(energy);

		// 功率 (基础单位: 瓦特)
		QuantityInfo power(QuantityType::Power, "Power", "Watt");
		power.units.emplace_back("Watt", "Watts", "W", 1.0);
		power.units.emplace_back("Kilowatt", "Kilowatts", "kW", 1000.0);
		power.units.emplace_back("Megawatt", "Megawatts", "MW", 1000000.0);
		power.units.emplace_back("Horsepower", "Horsepowers", "hp", 745.699872);
		power.units.emplace_back("MetricHorsepower", "MetricHorsepowers", "PS", 735.49875);
		quantities.push_back(power);

		// 压力 (基础单位: 帕斯卡)
		QuantityInfo pressure(QuantityType::Pressure, "Pressure", "Pascal");
		pressure.units.emplace_back("Pascal", "Pascals", "Pa", 1.0);
		pressure.units.emplace_back("Kilopascal", "Kilopascals", "kPa", 1000.0);
		pressure.units.emplace_back("Bar", "Bars", "bar", 100000.0);
		pressure.units.emplace_back("Atmosphere", "Atmospheres", "atm", 101325.0);
		pressure.units.emplace_back("PoundForcePerSquareInch", "PoundsForcePerSquareInch", "psi", 6894.757293);
		pressure.units.emplace_back("Torr", "Torrs", "Torr", 133.322);
		pressure.units.emplace_back("MillimeterOfMercury", "MillimetersOfMercury", "mmHg", 133.322);
		quantities.push_back(pressure);

		// 角度 (基础单位: 弧度)
		QuantityInfo angle(QuantityType::Angle, "Angle", "Radian");
		angle.units.emplace_back("Radian", "Radians", "rad", 1.0);
		angle.units.emplace_back("Degree", "Degrees", "°", M_PI / 180.0);
		angle.units.emplace_back("Gradian", "Gradians", "grad", M_PI / 200.0);
		angle.units.emplace_back("Revolution", "Revolutions", "rev", 2.0 * M_PI);
		quantities.push_back(angle);

		// 加速度 (基础单位: 米每二次方秒)
		QuantityInfo acceleration(QuantityType::Acceleration, "Acceleration", "MeterPerSecondSquared");
		acceleration.units.emplace_back("MeterPerSecondSquared", "MetersPerSecondSquared", "m/s²", 1.0);
		acceleration.units.emplace_back("StandardGravity", "StandardGravities", "g", 9.80665);
		acceleration.units.emplace_back("FootPerSecondSquared", "FeetPerSecondSquared", "ft/s²", 0.3048);
		quantities.push_back(acceleration);

		// 信息 (基础单位: 字节)
		QuantityInfo information(QuantityType::Information, "Information", "Byte");
		information.units.emplace_back("Bit", "Bits", "bit", 0.125);
		information.units.emplace_back("Byte", "Bytes", "B", 1.0);
		information.units.emplace_back("Kilobyte", "Kilobytes", "KB", 1000.0);
		information.units.emplace_back("Megabyte", "Megabytes", "MB", 1000000.0);
		information.units.emplace_back("Gigabyte", "Gigabytes", "GB", 1000000000.0);
		information.units.emplace_back("Terabyte", "Terabytes", "TB", 1000000000000.0);
		information.units.emplace_back("Kibibyte", "Kibibytes", "KiB", 1024.0);
		information.units.emplace_back("Mebibyte", "Mebibytes", "MiB", 1048576.0);
		information.units.emplace_back("Gibibyte", "Gibibytes", "GiB", 1073741824.0);
		information.units.emplace_back("Tebibyte", "Tebibytes", "TiB", 1099511627776.0);
		quantities.push_back(information);
	}
};
