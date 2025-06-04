#pragma once
#pragma execution_character_set("utf-8")
#include <string>
#include <vector>
#include <unordered_set>
#include <regex>
#include <algorithm>
#include <codecvt>
#include <locale>

#include "MainTools.hpp"


//static bool IsChinese(const std::wstring& str)
//{
//return std::ranges::any_of(str, [](wchar_t ch)
//{
//	return ch >= 0x4E00 && ch <= 0x9FFF;
//});
//	for (wchar_t ch : str)
//	{
//		if (ch >= 0x4E00 && ch <= 0x9FFF)
//			return true;
//	}
//	return false;
//}


class PinyinHelper
{
public:
	static std::wstring ConvertEnWordsToAbbrWords(const std::wstring& input)
	{
		return ConvertEnWordsToFirstChar(input);
	}


	static std::vector<std::wstring> HandlePolyphone(const std::vector<std::wstring>& array)
	{
		std::vector<std::wstring> result;
		std::vector<std::wstring> temp;

		for (const auto& item : array)
		{
			std::vector<std::wstring> splits;
			size_t pos = 0, found;
			while ((found = item.find(L' ', pos)) != std::wstring::npos)
			{
				splits.push_back(item.substr(pos, found - pos));
				pos = found + 1;
			}
			splits.push_back(item.substr(pos));

			temp.clear();
			if (!result.empty())
			{
				for (const auto& res : result)
				{
					for (const auto& split : splits)
					{
						temp.push_back(res + split);
					}
				}
			}
			else
			{
				for (const auto& split : splits)
				{
					temp.push_back(split);
				}
			}
			result = temp;
		}
		return result;
	}

	static std::vector<std::wstring> HandlePolyphone2(const std::vector<std::wstring>& array)
	{
		std::vector<std::wstring> result;
		std::vector<std::wstring> temp;

		for (const auto& item : array)
		{
			std::vector<std::wstring> splits;
			size_t pos = 0, found;
			while ((found = item.find(L' ', pos)) != std::wstring::npos)
			{
				splits.push_back(item.substr(pos, found - pos));
				pos = found + 1;
			}
			splits.push_back(item.substr(pos));

			temp.clear();
			if (!result.empty())
			{
				for (const auto& res : result)
				{
					for (const std::wstring& split : splits)
					{
						// temp.push_back(res + split[0]);
						if (!split.empty())
							temp.push_back(res + split.substr(0, 1));
						else
						{
							temp.push_back(res);
						}
					}
				}
			}
			else
			{
				for (const std::wstring& split : splits)
				{
					if (!split.empty())
					{
						// wchar_t temp2 = split[0];
						temp.push_back(split.substr(0, 1));
					}
					// temp.push_back(split);
				}
			}
			result = temp;
		}
		return result;
	}


	static std::vector<std::wstring> HandlePolyphone3(const std::vector<std::wstring>& array)
	{
		std::vector<std::wstring> result;
		std::vector<std::wstring> temp;

		for (const auto& item : array)
		{
			std::vector<wchar_t> initials;
			size_t pos = 0, found;
			while ((found = item.find(L' ', pos)) != std::wstring::npos)
			{
				if (found > pos)
					initials.push_back(item[pos]);
				pos = found + 1;
			}
			if (pos < item.size())
				initials.push_back(item[pos]);

			temp.clear();
			if (!result.empty())
			{
				for (const auto& res : result)
				{
					for (const wchar_t ch : initials)
					{
						temp.push_back(res + ch);
					}
				}
			}
			else
			{
				for (const wchar_t ch : initials)
				{
					temp.emplace_back(1, ch);
				}
			}
			result = temp;
		}
		return result;
	}


	static std::wstring ConvertEnWordsToFirstChar(const std::wstring& input)
	{
		std::wstring result;
		bool newWord = true;

		for (const wchar_t ch : input)
		{
			if (iswalnum(ch)) // 字母或数字
			{
				if (newWord)
				{
					result += ch;
					newWord = false;
				}
			}
			else
			{
				newWord = true;
			}
		}
		return result;
	}

	static std::wstring ConvertEnWordsToFourChar(const std::wstring& input)
	{
		std::wstring result;
		std::wstring word;

		for (const wchar_t ch : input)
		{
			if (iswalpha(ch)) // 只处理字母
			{
				word += ch;
			}
			else
			{
				if (word.length() >= 5)
				{
					result += word.substr(0, 3);
					result += word[word.length() - 1];
				}
				word.clear();
			}
		}

		// 最后一个单词处理
		if (word.length() >= 5)
		{
			result += word.substr(0, 3);
			result += word[word.length() - 1];
		}

		return result;
	}


	static std::vector<std::wstring> GetPinyin(const std::wstring& str, const bool polyphone)
	{
		std::vector<std::wstring> result;
		for (wchar_t ch : str)
		{
			if (IsChineseChar(ch))
			{
				std::wstring s(1, ch);
				auto pinyins = GetPinyinByOne(s, polyphone);
				if (!pinyins.empty())
				{
					// result.insert(result.end(), pinyins.begin(), pinyins.end());
					std::wstring oss;
					for (size_t i = 0; i < pinyins.size(); ++i)
					{
						if (i > 0) oss += L' '; // 添加空格分隔符
						oss += pinyins[i];
					}
					result.push_back(oss);
				}
				else
				{
					result.push_back(s);
				}
			}
			else if (ch == L' ')
			{
				// 这里有问题，如果是vs2022 编译的，如果把空格也添加到数组里，就会把结果产生一个中文字符，很多很多个
			}
			else
			{
				//result.emplace_back(&ch);
				std::wstring oss;
				oss += ch;
				result.push_back(oss);
			}
		}
		return result;
	}

	static std::vector<std::wstring> GetPinyinFirstEnChar(const std::wstring& str, const bool polyphone)
	{
		std::vector<std::wstring> result;
		bool firstWord = true;
		for (const wchar_t ch : str)
		{
			if (IsChineseChar(ch))
			{
				firstWord = true;
				std::wstring s(1, ch);
				auto pinyins = GetPinyinByOne(s, polyphone);
				if (!pinyins.empty())
				{
					// result.insert(result.end(), pinyins.begin(), pinyins.end());
					std::wstring oss;
					for (size_t i = 0; i < pinyins.size(); ++i)
					{
						if (i > 0) oss += L" "; // 添加空格分隔符
						oss += pinyins[i];
					}
					result.push_back(oss);
				}
				else
				{
					result.push_back(s);
				}
			}
			else if (ch == L' ')
			{
				firstWord = true;
				//result.emplace_back(&ch);
				//std::wstring oss;
				//oss += ch;
				//result.push_back(oss);
			}
			else
			{
				if (firstWord)
				{
					//result.emplace_back(&ch);
					std::wstring oss;
					oss += ch;
					result.push_back(oss);

					firstWord = false;
				}
			}
		}
		return result;
	}


	static std::vector<std::wstring> GetPinyinByOne(const std::wstring& ch, const bool polyphone)
	{
		std::vector<std::wstring> result;
		for (size_t i = 0; i < HanCharArr.size(); ++i)
		{
			const std::wstring& hanGroup = HanCharArr[i];
			if (hanGroup.find(ch) != std::wstring::npos)
			{
				result.push_back(XiaoHePinyinArr[i]);
				if (!polyphone)
					return result;
			}
		}
		return result;
	}

	// static bool IsChinese(const std::wstring& str)
	// {
	// 	static const std::wregex reg(LR"([\x4e00-\x9fff])");
	// 	return std::regex_match(str, reg);
	// }
	// static bool IsChinese(const std::wstring& str)
	// {
	// 	static const std::wregex reg(LR"([\x4e00-\x9fff])");
	// 	return std::regex_search(str, reg); // ✅ 匹配字符串中是否包含汉字
	// }
	// static bool IsChinese(const std::wstring& str)
	// {
	// 	if (str.empty()) return false;
	// 	for (wchar_t ch : str)
	// 	{
	// 		if (ch >= 0x4E00 && ch <= 0x9FFF)
	// 			return true;
	// 	}
	// 	return false;
	// }

	static std::wstring GetPinyinLongStr(const std::wstring& str)
	{
		// 拆分中英文
		std::wstring enStr, chStr;
		for (wchar_t ch : str)
		{
			if (IsChineseChar(ch))
				chStr += ch;
			else
				enStr += ch;
		}

		// std::vector<std::wstring> mainArr = GetPinyinAll(str, true);
		// std::vector<std::wstring> chArr = GetPinyinJustCh(chStr, true);

		// 拼音组合部分
		// std::unordered_set<std::wstring> resultSet;
		// for (const auto& item : HandlePolyphone(mainArr)) {
		// 	resultSet.insert(ToLower(item));
		// }
		// for (const auto& item : HandlePolyphone2(chArr)) {
		// 	resultSet.insert(ToLower(item));
		// }

		// GetPinyin

		// std::wstring result = enStr;
		std::wstring result;
		// for (const auto& item : resultSet) {
		// 	result += item + L" ";
		// }
		const auto strPinyins = GetPinyin(str, true);
		const auto chstrPinyins = GetPinyinFirstEnChar(str, true);

		std::unordered_set<std::wstring> resultSet;
		for (const auto& item : HandlePolyphone(strPinyins))
		{
			resultSet.insert(item);
		}
		if (chstrPinyins.size() > 1)
		{
			const std::vector<std::wstring> basic_strings = HandlePolyphone2(chstrPinyins);
			for (const auto& item : basic_strings)
			{
				resultSet.insert(item);
			}
		}

		for (const auto& item : resultSet)
		{
			result += item + L" ";
		}


		// result.insert(result.end(), temp.begin(), temp.end());
		// result.insert(result.end(), temp2.begin(), temp2.end());

		// for (const auto& pinyin : temp)
		// {
		// 	result += pinyin;
		// }
		// for (const auto& pinyin : temp2)
		// {
		// 	result += pinyin;
		// }


		const std::wstring convert_en_words_to_abbr_words = ConvertEnWordsToAbbrWords(enStr);
		// 添加英文部分的缩写形式
		if (convert_en_words_to_abbr_words.length() > 1)
			result += convert_en_words_to_abbr_words + L" ";

		const std::wstring input = ConvertEnWordsToFourChar(enStr);
		if (input.length() > 1)
			result += input;

		// 去除多余空格
		return MyTrim(result);
	}

private:
	static inline const std::vector<std::wstring> HanCharArr = {
		L"阿啊呵腌嗄吖锕",
		L"额阿俄恶鹅遏鄂厄饿峨扼娥鳄哦蛾噩愕讹锷垩婀鹗萼谔莪腭锇颚呃阏屙苊轭",
		L"爱埃艾碍癌哀挨矮隘蔼唉皑哎霭捱暧嫒嗳瑷嗌锿砹",
		L"诶",
		L"系西席息希习吸喜细析戏洗悉锡溪惜稀袭夕洒晰昔牺腊烯熙媳栖膝隙犀蹊硒兮熄曦禧嬉玺奚汐徙羲铣淅嘻歙熹矽蟋郗唏皙隰樨浠忾蜥檄郄翕阋鳃舾屣葸螅咭粞觋欷僖醯鼷裼穸饩舄禊诶菥蓰晞",
		L"一以已意议义益亿易医艺食依移衣异伊仪宜射遗疑毅谊亦疫役忆抑尾乙译翼蛇溢椅沂泄逸蚁夷邑怡绎彝裔姨熠贻矣屹颐倚诣胰奕翌疙弈轶蛾驿壹猗臆弋铱旖漪迤佚翊诒怿痍懿饴峄揖眙镒仡黟肄咿翳挹缢呓刈咦嶷羿钇殪荑薏蜴镱噫癔苡悒嗌瘗衤佾埸圯舣酏劓燚晹祎係",
		L"安案按岸暗鞍氨俺胺铵谙庵黯鹌桉埯犴揞厂广",
		L"厂汉韩含旱寒汗涵函喊憾罕焊翰邯撼瀚憨捍酣悍鼾邗颔蚶晗菡旰顸犴焓撖琀浛",
		L"昂仰盎肮",
		L"奥澳傲熬凹鳌敖遨鏖袄坳翱嗷拗懊岙螯骜獒鏊艹媪廒聱奡",
		L"瓦挖娃洼袜蛙凹哇佤娲呙腽",
		L"于与育余预域予遇奥语誉玉鱼雨渔裕愈娱欲吁舆宇羽逾豫郁寓吾狱喻御浴愉禹俞邪榆愚渝尉淤虞屿峪粥驭瑜禺毓钰隅芋熨瘀迂煜昱汩於臾盂聿竽萸妪腴圄谕觎揄龉谀俣馀庾妤瘐鬻欤鹬阈嵛雩鹆圉蜮伛纡窬窳饫蓣狳肀舁蝓燠",
		L"牛纽扭钮拗妞忸狃",
		L"哦噢喔嚄",
		L"把八巴拔伯吧坝爸霸罢芭跋扒叭靶疤笆耙鲅粑岜灞钯捌菝魃茇",
		L"怕帕爬扒趴琶啪葩耙杷钯筢掱",
		L"被批副否皮坏辟啤匹披疲罢僻毗坯脾譬劈媲屁琵邳裨痞癖陂丕枇噼霹吡纰砒铍淠郫埤濞睥芘蚍圮鼙罴蜱疋貔仳庀擗甓陴玭潎",
		L"比必币笔毕秘避闭佛辟壁弊彼逼碧鼻臂蔽拂泌璧庇痹毙弼匕鄙陛裨贲敝蓖吡篦纰俾铋毖筚荸薜婢哔跸濞秕荜愎睥妣芘箅髀畀滗狴萆嬖襞舭赑",
		L"百白败摆伯拜柏佰掰呗擘捭稗",
		L"波博播勃拨薄佛伯玻搏柏泊舶剥渤卜驳簿脖膊簸菠礴箔铂亳钵帛擘饽跛钹趵檗啵鹁擗踣",
		L"北被备倍背杯勃贝辈悲碑臂卑悖惫蓓陂钡狈呗焙碚褙庳鞴孛鹎邶鐾",
		L"办版半班般板颁伴搬斑扮拌扳瓣坂阪绊钣瘢舨癍",
		L"判盘番潘攀盼拚畔胖叛拌蹒磐爿蟠泮袢襻丬",
		L"份宾频滨斌彬濒殡缤鬓槟摈膑玢镔豳髌傧",
		L"帮邦彭旁榜棒膀镑绑傍磅蚌谤梆浜蒡",
		L"旁庞乓磅螃彷滂逄耪",
		L"泵崩蚌蹦迸绷甭嘣甏堋",
		L"报保包宝暴胞薄爆炮饱抱堡剥鲍曝葆瀑豹刨褒雹孢苞煲褓趵鸨龅勹",
		L"不部步布补捕堡埔卜埠簿哺怖钚卟瓿逋晡醭钸",
		L"普暴铺浦朴堡葡谱埔扑仆蒲曝瀑溥莆圃璞濮菩蹼匍噗氆攵镨攴镤",
		L"面棉免绵缅勉眠冕娩腼渑湎沔黾宀眄",
		L"破繁坡迫颇朴泊婆泼魄粕鄱珀陂叵笸泺皤钋钷",
		L"反范犯繁饭泛翻凡返番贩烦拚帆樊藩矾梵蕃钒幡畈蘩蹯燔",
		L"府服副负富复福夫妇幅付扶父符附腐赴佛浮覆辅傅伏抚赋辐腹弗肤阜袱缚甫氟斧孚敷俯拂俘咐腑孵芙涪釜脯茯馥宓绂讣呋罘麸蝠匐芾蜉跗凫滏蝮驸绋蚨砩桴赙菔呒趺苻拊阝鲋怫稃郛莩幞祓艴黻黼鳆",
		L"本体奔苯笨夯贲锛畚坌犇",
		L"风丰封峰奉凤锋冯逢缝蜂枫疯讽烽俸沣酆砜葑唪",
		L"变便边编遍辩鞭辨贬匾扁卞汴辫砭苄蝙鳊弁窆笾煸褊碥忭缏",
		L"便片篇偏骗翩扁骈胼蹁谝犏缏",
		L"镇真针圳振震珍阵诊填侦臻贞枕桢赈祯帧甄斟缜箴疹砧榛鸩轸稹溱蓁胗椹朕畛浈",
		L"表标彪镖裱飚膘飙镳婊骠飑杓髟鳔灬瘭猋骉",
		L"票朴漂飘嫖瓢剽缥殍瞟骠嘌莩螵",
		L"和活或货获火伙惑霍祸豁嚯藿锪蠖钬耠镬夥灬劐攉嚄喐嚿",
		L"别鳖憋瘪蹩",
		L"民敏闽闵皿泯岷悯珉抿黾缗玟愍苠鳘",
		L"分份纷奋粉氛芬愤粪坟汾焚酚吩忿棼玢鼢瀵偾鲼瞓",
		L"并病兵冰屏饼炳秉丙摒柄槟禀枋邴冫靐抦",
		L"更耕颈庚耿梗埂羹哽赓绠鲠",
		L"方放房防访纺芳仿坊妨肪邡舫彷枋鲂匚钫昉",
		L"现先县见线限显险献鲜洗宪纤陷闲贤仙衔掀咸嫌掺羡弦腺痫娴舷馅酰铣冼涎暹籼锨苋蚬跹岘藓燹鹇氙莶霰跣猃彡祆筅顕鱻咁",
		L"不否缶",
		L"拆擦嚓礤",
		L"查察差茶插叉刹茬楂岔诧碴嚓喳姹杈汊衩搽槎镲苴檫馇锸猹",
		L"才采财材菜彩裁蔡猜踩睬啋",
		L"参残餐灿惨蚕掺璨惭粲孱骖黪",
		L"信深参身神什审申甚沈伸慎渗肾绅莘呻婶娠砷蜃哂椹葚吲糁渖诜谂矧胂珅屾燊",
		L"参岑涔",
		L"三参散伞叁糁馓毵",
		L"藏仓苍沧舱臧伧",
		L"藏脏葬赃臧奘驵",
		L"称陈沈沉晨琛臣尘辰衬趁忱郴宸谌碜嗔抻榇伧谶龀肜棽",
		L"草操曹槽糙嘈漕螬艚屮",
		L"策测册侧厕栅恻",
		L"责则泽择侧咋啧仄箦赜笮舴昃迮帻",
		L"债择齐宅寨侧摘窄斋祭翟砦瘵哜",
		L"到道导岛倒刀盗稻蹈悼捣叨祷焘氘纛刂帱忉",
		L"层曾蹭噌",
		L"查扎炸诈闸渣咋乍榨楂札栅眨咤柞喳喋铡蚱吒怍砟揸痄哳齄",
		L"差拆柴钗豺侪虿瘥",
		L"次此差词辞刺瓷磁兹慈茨赐祠伺雌疵鹚糍呲粢",
		L"资自子字齐咨滋仔姿紫兹孜淄籽梓鲻渍姊吱秭恣甾孳訾滓锱辎趑龇赀眦缁呲笫谘嵫髭茈粢觜耔",
		L"措错磋挫搓撮蹉锉厝嵯痤矬瘥脞鹾",
		L"产单阐崭缠掺禅颤铲蝉搀潺蟾馋忏婵孱觇廛谄谗澶骣羼躔蒇冁",
		L"山单善陕闪衫擅汕扇掺珊禅删膳缮赡鄯栅煽姗跚鳝嬗潸讪舢苫疝掸膻钐剡蟮芟埏彡骟羴",
		L"展战占站崭粘湛沾瞻颤詹斩盏辗绽毡栈蘸旃谵搌",
		L"新心信辛欣薪馨鑫芯锌忻莘昕衅歆囟忄镡馫",
		L"联连练廉炼脸莲恋链帘怜涟敛琏镰濂楝鲢殓潋裢裣臁奁莶蠊蔹",
		L"场长厂常偿昌唱畅倡尝肠敞倘猖娼淌裳徜昶怅嫦菖鲳阊伥苌氅惝鬯玚",
		L"长张章障涨掌帐胀彰丈仗漳樟账杖璋嶂仉瘴蟑獐幛鄣嫜",
		L"超朝潮炒钞抄巢吵剿绰嘲晁焯耖怊",
		L"着照招找召朝赵兆昭肇罩钊沼嘲爪诏濯啁棹笊",
		L"调州周洲舟骤轴昼宙粥皱肘咒帚胄绉纣妯啁诌繇碡籀酎荮",
		L"车彻撤尺扯澈掣坼砗屮唓",
		L"车局据具举且居剧巨聚渠距句拒俱柜菊拘炬桔惧矩鞠驹锯踞咀瞿枸掬沮莒橘飓疽钜趄踽遽琚龃椐苣裾榘狙倨榉苴讵雎锔窭鞫犋屦醵",
		L"成程城承称盛抢乘诚呈净惩撑澄秤橙骋逞瞠丞晟铛埕塍蛏柽铖酲裎枨琤",
		L"容荣融绒溶蓉熔戎榕茸冗嵘肜狨蝾瑢",
		L"生声升胜盛乘圣剩牲甸省绳笙甥嵊晟渑眚焺珄昇湦陹竔琞",
		L"等登邓灯澄凳瞪蹬噔磴嶝镫簦戥",
		L"制之治质职只志至指织支值知识直致执置止植纸拓智殖秩旨址滞氏枝芝脂帜汁肢挚稚酯掷峙炙栉侄芷窒咫吱趾痔蜘郅桎雉祉郦陟痣蛭帙枳踯徵胝栀贽祗豸鸷摭轵卮轾彘觯絷跖埴夂黹忮骘膣踬臸",
		L"政正证争整征郑丁症挣蒸睁铮筝拯峥怔诤狰徵钲掟",
		L"堂唐糖汤塘躺趟倘棠烫淌膛搪镗傥螳溏帑羰樘醣螗耥铴瑭",
		L"持吃池迟赤驰尺斥齿翅匙痴耻炽侈弛叱啻坻眙嗤墀哧茌豉敕笞饬踟蚩柢媸魑篪褫彳鸱螭瘛眵傺",
		L"是时实事市十使世施式势视识师史示石食始士失适试什泽室似诗饰殖释驶氏硕逝湿蚀狮誓拾尸匙仕柿矢峙侍噬嗜栅拭嘘屎恃轼虱耆舐莳铈谥炻豕鲥饣螫酾筮埘弑礻蓍鲺贳湜",
		L"企其起期气七器汽奇齐启旗棋妻弃揭枝歧欺骑契迄亟漆戚岂稽岐琦栖缉琪泣乞砌祁崎绮祺祈凄淇杞脐麒圻憩芪伎俟畦耆葺沏萋骐鳍綦讫蕲屺颀亓碛柒啐汔綮萁嘁蛴槭欹芑桤丌蜞",
		L"揣踹啜搋膪",
		L"托脱拓拖妥驼陀沱鸵驮唾椭坨佗砣跎庹柁橐乇铊沲酡鼍箨柝",
		L"多度夺朵躲铎隋咄堕舵垛惰哆踱跺掇剁柁缍沲裰哚隳",
		L"学血雪削薛穴靴谑噱鳕踅泶彐",
		L"重种充冲涌崇虫宠忡憧舂茺铳艟蟲翀",
		L"筹抽绸酬愁丑臭仇畴稠瞅踌惆俦瘳雠帱",
		L"求球秋丘邱仇酋裘龟囚遒鳅虬蚯泅楸湫犰逑巯艽俅蝤赇鼽糗",
		L"修秀休宿袖绣臭朽锈羞嗅岫溴庥馐咻髹鸺貅飍",
		L"出处础初助除储畜触楚厨雏矗橱锄滁躇怵绌搐刍蜍黜杵蹰亍樗憷楮",
		L"团揣湍疃抟彖",
		L"追坠缀揣椎锥赘惴隹骓缒",
		L"传川船穿串喘椽舛钏遄氚巛舡",
		L"专转传赚砖撰篆馔啭颛孨",
		L"元员院原源远愿园援圆缘袁怨渊苑宛冤媛猿垣沅塬垸鸳辕鸢瑗圜爰芫鼋橼螈眢箢掾厵",
		L"窜攒篡蹿撺爨汆镩",
		L"创床窗闯幢疮怆",
		L"装状庄壮撞妆幢桩奘僮戆壵",
		L"吹垂锤炊椎陲槌捶棰",
		L"春纯醇淳唇椿蠢鹑朐莼肫蝽",
		L"准屯淳谆肫窀",
		L"促趋趣粗簇醋卒蹴猝蹙蔟殂徂麤",
		L"吨顿盾敦蹲墩囤沌钝炖盹遁趸砘礅",
		L"区去取曲趋渠趣驱屈躯衢娶祛瞿岖龋觑朐蛐癯蛆苣阒诎劬蕖蘧氍黢蠼璩麴鸲磲佢",
		L"需许续须序徐休蓄畜虚吁绪叙旭邪恤墟栩絮圩婿戌胥嘘浒煦酗诩朐盱蓿溆洫顼勖糈砉醑媭珝昫",
		L"辍绰戳淖啜龊踔辶",
		L"组族足祖租阻卒俎诅镞菹",
		L"济机其技基记计系期际及集级几给积极己纪即继击既激绩急奇吉季齐疾迹鸡剂辑籍寄挤圾冀亟寂暨脊跻肌稽忌饥祭缉棘矶汲畸姬藉瘠骥羁妓讥稷蓟悸嫉岌叽伎鲫诘楫荠戟箕霁嵇觊麂畿玑笈犄芨唧屐髻戢佶偈笄跽蒺乩咭赍嵴虮掎齑殛鲚剞洎丌墼蕺彐芰哜",
		L"从丛匆聪葱囱琮淙枞骢苁璁",
		L"总从综宗纵踪棕粽鬃偬枞腙",
		L"凑辏腠楱",
		L"衰催崔脆翠萃粹摧璀瘁悴淬啐隹毳榱",
		L"为位委未维卫围违威伟危味微唯谓伪慰尾魏韦胃畏帷喂巍萎蔚纬潍尉渭惟薇苇炜圩娓诿玮崴桅偎逶倭猥囗葳隗痿猬涠嵬韪煨艉隹帏闱洧沩隈鲔軎烓",
		L"村存寸忖皴",
		L"作做座左坐昨佐琢撮祚柞唑嘬酢怍笮阼胙咗",
		L"钻纂攥缵躜",
		L"大达打答搭沓瘩惮嗒哒耷鞑靼褡笪怛妲龘",
		L"大代带待贷毒戴袋歹呆隶逮岱傣棣怠殆黛甙埭诒绐玳呔迨",
		L"大台太态泰抬胎汰钛苔薹肽跆邰鲐酞骀炱",
		L"他它她拓塔踏塌榻沓漯獭嗒挞蹋趿遢铊鳎溻闼譶",
		L"但单石担丹胆旦弹蛋淡诞氮郸耽殚惮儋眈疸澹掸膻啖箪聃萏瘅赕",
		L"路六陆录绿露鲁卢炉鹿禄赂芦庐碌麓颅泸卤潞鹭辘虏璐漉噜戮鲈掳橹轳逯渌蓼撸鸬栌氇胪镥簏舻辂垆",
		L"谈探坦摊弹炭坛滩贪叹谭潭碳毯瘫檀痰袒坍覃忐昙郯澹钽锬",
		L"人任认仁忍韧刃纫饪妊荏稔壬仞轫亻衽",
		L"家结解价界接节她届介阶街借杰洁截姐揭捷劫戒皆竭桔诫楷秸睫藉拮芥诘碣嗟颉蚧孑婕疖桀讦疥偈羯袷哜喈卩鲒骱劼吤",
		L"研严验演言眼烟沿延盐炎燕岩宴艳颜殷彦掩淹阎衍铅雁咽厌焰堰砚唁焉晏檐蜒奄俨腌妍谚兖筵焱偃闫嫣鄢湮赝胭琰滟阉魇酽郾恹崦芫剡鼹菸餍埏谳讠厣罨啱",
		L"当党档荡挡宕砀铛裆凼菪谠氹",
		L"套讨跳陶涛逃桃萄淘掏滔韬叨洮啕绦饕鼗弢",
		L"条调挑跳迢眺苕窕笤佻啁粜髫铫祧龆蜩鲦",
		L"特忑忒铽慝",
		L"的地得德底锝",
		L"得",
		L"的地第提低底抵弟迪递帝敌堤蒂缔滴涤翟娣笛棣荻谛狄邸嘀砥坻诋嫡镝碲骶氐柢籴羝睇觌",
		L"体提题弟替梯踢惕剔蹄棣啼屉剃涕锑倜悌逖嚏荑醍绨鹈缇裼",
		L"推退弟腿褪颓蜕忒煺",
		L"有由又优游油友右邮尤忧幼犹诱悠幽佑釉柚铀鱿囿酉攸黝莠猷蝣疣呦蚴莸莜铕宥繇卣牖鼬尢蚰侑甴",
		L"电点店典奠甸碘淀殿垫颠滇癫巅惦掂癜玷佃踮靛钿簟坫阽",
		L"天田添填甜甸恬腆佃舔钿阗忝殄畋栝掭瑱",
		L"主术住注助属逐宁著筑驻朱珠祝猪诸柱竹铸株瞩嘱贮煮烛苎褚蛛拄铢洙竺蛀渚伫杼侏澍诛茱箸炷躅翥潴邾槠舳橥丶瘃麈疰",
		L"年念酿辗碾廿捻撵拈蔫鲶埝鲇辇黏惗唸",
		L"调掉雕吊钓***貂凋碉鲷叼铫铞",
		L"要么约药邀摇耀腰遥姚窑瑶咬尧钥谣肴夭侥吆疟妖幺杳舀窕窈曜鹞爻繇徭轺铫鳐崾珧垚峣",
		L"跌叠蝶迭碟爹谍牒耋佚喋堞瓞鲽垤揲蹀",
		L"设社摄涉射折舍蛇拾舌奢慑赦赊佘麝歙畲厍猞揲滠",
		L"业也夜叶射野液冶喝页爷耶邪咽椰烨掖拽曳晔谒腋噎揶靥邺铘揲嘢",
		L"些解协写血叶谢械鞋胁斜携懈契卸谐泄蟹邪歇泻屑挟燮榭蝎撷偕亵楔颉缬邂鲑瀣勰榍薤绁渫廨獬躞劦",
		L"这者着著浙折哲蔗遮辙辄柘锗褶蜇蛰鹧谪赭摺乇磔螫晢喆嚞嗻啫",
		L"定订顶丁鼎盯钉锭叮仃铤町酊啶碇腚疔玎耵掟",
		L"丢铥",
		L"听庭停厅廷挺亭艇婷汀铤烃霆町蜓葶梃莛",
		L"动东董冬洞懂冻栋侗咚峒氡恫胴硐垌鸫岽胨",
		L"同通统童痛铜桶桐筒彤侗佟潼捅酮砼瞳恸峒仝嗵僮垌茼",
		L"中重种众终钟忠仲衷肿踵冢盅蚣忪锺舯螽夂",
		L"都斗读豆抖兜陡逗窦渎蚪痘蔸钭篼唞",
		L"度都独督读毒渡杜堵赌睹肚镀渎笃竺嘟犊妒牍蠹椟黩芏髑",
		L"断段短端锻缎煅椴簖",
		L"对队追敦兑堆碓镦怼憝",
		L"瑞兑锐睿芮蕊蕤蚋枘",
		L"月说约越乐跃兑阅岳粤悦曰钥栎钺樾瀹龠哕刖曱",
		L"吞屯囤褪豚臀饨暾氽",
		L"会回挥汇惠辉恢徽绘毁慧灰贿卉悔秽溃荟晖彗讳诲珲堕诙蕙晦睢麾烩茴喙桧蛔洄浍虺恚蟪咴隳缋哕烜翙",
		L"务物无五武午吴舞伍污乌误亡恶屋晤悟吾雾芜梧勿巫侮坞毋诬呜钨邬捂鹜兀婺妩於戊鹉浯蜈唔骛仵焐芴鋈庑鼯牾怃圬忤痦迕杌寤阢沕",
		L"亚压雅牙押鸭呀轧涯崖邪芽哑讶鸦娅衙丫蚜碣垭伢氩桠琊揠吖睚痖疋迓岈砑",
		L"和合河何核盖贺喝赫荷盒鹤吓呵苛禾菏壑褐涸阂阖劾诃颌嗬貉曷翮纥盍翯",
		L"我握窝沃卧挝涡斡渥幄蜗喔倭莴龌肟硪",
		L"恩摁蒽奀",
		L"嗯唔",
		L"而二尔儿耳迩饵洱贰铒珥佴鸸鲕",
		L"发法罚乏伐阀筏砝垡珐",
		L"全权券泉圈拳劝犬铨痊诠荃醛蜷颧绻犭筌鬈悛辁畎",
		L"费非飞肥废菲肺啡沸匪斐蜚妃诽扉翡霏吠绯腓痱芾淝悱狒榧砩鲱篚镄飝",
		L"配培坏赔佩陪沛裴胚妃霈淠旆帔呸醅辔锫",
		L"平评凭瓶冯屏萍苹乒坪枰娉俜鲆",
		L"佛",
		L"和护许户核湖互乎呼胡戏忽虎沪糊壶葫狐蝴弧瑚浒鹄琥扈唬滹惚祜囫斛笏芴醐猢怙唿戽槲觳煳鹕冱瓠虍岵鹱烀轷",
		L"夹咖嘎尬噶旮伽尕钆尜呷",
		L"个合各革格歌哥盖隔割阁戈葛鸽搁胳舸疙铬骼蛤咯圪镉颌仡硌嗝鬲膈纥袼搿塥哿虼吤嗰",
		L"哈蛤铪",
		L"下夏峡厦辖霞夹虾狭吓侠暇遐瞎匣瑕唬呷黠硖罅狎瘕柙",
		L"改该盖概溉钙丐芥赅垓陔戤",
		L"海还害孩亥咳骸骇氦嗨胲醢",
		L"干感赶敢甘肝杆赣乾柑尴竿秆橄矸淦苷擀酐绀泔坩旰疳澉",
		L"港钢刚岗纲冈杠缸扛肛罡戆筻",
		L"将强江港奖讲降疆蒋姜浆匠酱僵桨绛缰犟豇礓洚茳糨耩",
		L"行航杭巷夯吭桁沆绗颃",
		L"工公共供功红贡攻宫巩龚恭拱躬弓汞蚣珙觥肱廾",
		L"红宏洪轰虹鸿弘哄烘泓訇蕻闳讧荭黉薨轟",
		L"广光逛潢犷胱咣桄",
		L"穷琼穹邛茕筇跫蛩銎",
		L"高告搞稿膏糕镐皋羔锆杲郜睾诰藁篙缟槁槔",
		L"好号毫豪耗浩郝皓昊皋蒿壕灏嚎濠蚝貉颢嗥薅嚆",
		L"理力利立里李历例离励礼丽黎璃厉厘粒莉梨隶栗荔沥犁漓哩狸藜罹篱鲤砺吏澧俐骊溧砾莅锂笠蠡蛎痢雳俪傈醴栎郦俚枥喱逦娌鹂戾砬唳坜疠蜊黧猁鬲粝蓠呖跞疬缡鲡鳢嫠詈悝苈篥轹刕嚟",
		L"家加价假佳架甲嘉贾驾嫁夹稼钾挟拮迦伽颊浃枷戛荚痂颉镓笳珈岬胛袈郏葭袷瘕铗跏蛱恝哿",
		L"落罗络洛逻螺锣骆萝裸漯烙摞骡咯箩珞捋荦硌雒椤镙跞瘰泺脶猡倮蠃囖啰",
		L"可科克客刻课颗渴壳柯棵呵坷恪苛咳磕珂稞瞌溘轲窠嗑疴蝌岢铪颏髁蚵缂氪骒钶锞",
		L"卡恰洽掐髂袷咭葜",
		L"给",
		L"根跟亘艮哏茛",
		L"很狠恨痕哏",
		L"构购够句沟狗钩拘勾苟垢枸篝佝媾诟岣彀缑笱鞲觏遘玽",
		L"口扣寇叩抠佝蔻芤眍筘",
		L"股古顾故固鼓骨估谷贾姑孤雇辜菇沽咕呱锢钴箍汩梏痼崮轱鸪牯蛊诂毂鹘菰罟嘏臌觚瞽蛄酤牿鲴",
		L"牌排派拍迫徘湃俳哌蒎",
		L"括挂瓜刮寡卦呱褂剐胍诖鸹栝呙啩",
		L"投头透偷愉骰亠",
		L"怪拐乖",
		L"会快块筷脍蒯侩浍郐蒉狯哙",
		L"关管观馆官贯冠惯灌罐莞纶棺斡矜倌鹳鳏盥掼涫",
		L"万完晚湾玩碗顽挽弯蔓丸莞皖宛婉腕蜿惋烷琬畹豌剜纨绾脘菀芄箢",
		L"呢哪呐讷疒",
		L"规贵归轨桂柜圭鬼硅瑰跪龟匮闺诡癸鳜桧皈鲑刽晷傀眭妫炅庋簋刿宄匦攰",
		L"军均俊君峻菌竣钧骏龟浚隽郡筠皲麇捃",
		L"窘炯迥炅冂扃",
		L"决绝角觉掘崛诀獗抉爵嚼倔厥蕨攫珏矍蹶谲镢鳜噱桷噘撅橛孓觖劂爝",
		L"滚棍辊衮磙鲧绲丨",
		L"婚混魂浑昏棍珲荤馄诨溷阍",
		L"国过果郭锅裹帼涡椁囗蝈虢聒埚掴猓崞蜾呙馘",
		L"黑嘿嗨",
		L"看刊勘堪坎砍侃嵌槛瞰阚龛戡凵莰",
		L"衡横恒亨哼珩桁蘅",
		L"万没么模末冒莫摩墨默磨摸漠脉膜魔沫陌抹寞蘑摹蓦馍茉嘿谟秣蟆貉嫫镆殁耱嬷麽瘼貊貘尛",
		L"鹏朋彭膨蓬碰苹棚捧亨烹篷澎抨硼怦砰嘭蟛堋",
		L"后候厚侯猴喉吼逅篌糇骺後鲎瘊堠",
		L"化华划话花画滑哗豁骅桦猾铧砉舙",
		L"怀坏淮徊槐踝",
		L"还环换欢患缓唤焕幻痪桓寰涣宦垸洹浣豢奂郇圜獾鲩鬟萑逭漶锾缳擐嬛",
		L"讯训迅孙寻询循旬巡汛勋逊熏徇浚殉驯鲟薰荀浔洵峋埙巽郇醺恂荨窨蕈曛獯灥",
		L"黄荒煌皇凰慌晃潢谎惶簧璜恍幌湟蝗磺隍徨遑肓篁鳇蟥癀",
		L"能乃奶耐奈鼐萘氖柰佴艿妳",
		L"乱卵滦峦鸾栾銮挛孪脔娈",
		L"切且契窃茄砌锲怯伽惬妾趄挈郄箧慊",
		L"建间件见坚检健监减简艰践兼鉴键渐柬剑尖肩舰荐箭浅剪俭碱茧奸歼拣捡煎贱溅槛涧堑笺谏饯锏缄睑謇蹇腱菅翦戬毽笕犍硷鞯牮枧湔鲣囝裥踺搛缣鹣蒹谫僭戋趼楗",
		L"南难男楠喃囡赧腩囝蝻婻暔",
		L"前千钱签潜迁欠纤牵浅遣谦乾铅歉黔谴嵌倩钳茜虔堑钎骞阡掮钤扦芊犍荨仟芡悭缱佥愆褰凵肷岍搴箝慊椠汧",
		L"强抢疆墙枪腔锵呛羌蔷襁羟跄樯戕嫱戗炝镪锖蜣",
		L"向项相想乡象响香降像享箱羊祥湘详橡巷翔襄厢镶飨饷缃骧芗庠鲞葙蟓",
		L"教交较校角觉叫脚缴胶轿郊焦骄浇椒礁佼蕉娇矫搅绞酵剿嚼饺窖跤蛟侥狡姣皎茭峤铰醮鲛湫徼鹪僬噍艽挢敫",
		L"着著缴桌卓捉琢灼浊酌拙茁涿镯淖啄濯焯倬擢斫棹诼浞禚",
		L"桥乔侨巧悄敲俏壳雀瞧翘窍峭锹撬荞跷樵憔鞘橇峤诮谯愀鞒硗劁缲",
		L"小效销消校晓笑肖削孝萧俏潇硝宵啸嚣霄淆哮筱逍姣箫骁枭哓绡蛸崤枵魈婋虓皛啋",
		L"司四思斯食私死似丝饲寺肆撕泗伺嗣祀厮驷嘶锶俟巳蛳咝耜笥纟糸鸶缌澌姒汜厶兕",
		L"开凯慨岂楷恺揩锴铠忾垲剀锎蒈嘅",
		L"进金今近仅紧尽津斤禁锦劲晋谨筋巾浸襟靳瑾烬缙钅矜觐堇馑荩噤廑妗槿赆衿卺",
		L"亲勤侵秦钦琴禽芹沁寝擒覃噙矜嗪揿溱芩衾廑锓吣檎螓骎",
		L"经京精境竞景警竟井惊径静劲敬净镜睛晶颈荆兢靖泾憬鲸茎腈菁胫阱旌粳靓痉箐儆迳婧肼刭弪獍璟誩競",
		L"应营影英景迎映硬盈赢颖婴鹰荧莹樱瑛蝇萦莺颍膺缨瀛楹罂荥萤鹦滢蓥郢茔嘤璎嬴瘿媵撄潆煐媖暎锳",
		L"就究九酒久救旧纠舅灸疚揪咎韭玖臼柩赳鸠鹫厩啾阄桕僦鬏",
		L"最罪嘴醉咀蕞觜",
		L"卷捐圈眷娟倦绢隽镌涓鹃鄄蠲狷锩桊",
		L"算酸蒜狻",
		L"员运云允孕蕴韵酝耘晕匀芸陨纭郧筠恽韫郓氲殒愠昀菀狁沄",
		L"群裙逡麇",
		L"卡喀咖咔咯佧胩",
		L"康抗扛慷炕亢糠伉钪闶",
		L"坑铿吭",
		L"考靠烤拷铐栲尻犒",
		L"肯垦恳啃龈裉",
		L"因引银印音饮阴隐姻殷淫尹荫吟瘾寅茵圻垠鄞湮蚓氤胤龈窨喑铟洇狺夤廴吲霪茚堙",
		L"空控孔恐倥崆箜",
		L"苦库哭酷裤枯窟挎骷堀绔刳喾",
		L"跨夸垮挎胯侉",
		L"亏奎愧魁馈溃匮葵窥盔逵睽馗聩喟夔篑岿喹揆隗傀暌跬蒉愦悝蝰",
		L"款宽髋",
		L"况矿框狂旷眶匡筐邝圹哐贶夼诳诓纩",
		L"确却缺雀鹊阙瘸榷炔阕悫慤",
		L"困昆坤捆琨锟鲲醌髡悃阃焜鹍堃",
		L"扩括阔廓蛞",
		L"拉落垃腊啦辣蜡喇剌旯砬邋瘌",
		L"来莱赖睐徕籁涞赉濑癞崃疠铼",
		L"兰览蓝篮栏岚烂滥缆揽澜拦懒榄斓婪阑褴罱啉谰镧漤",
		L"林临邻赁琳磷淋麟霖鳞凛拎遴蔺吝粼嶙躏廪檩啉辚膦瞵懔",
		L"浪朗郎廊狼琅榔螂阆锒莨啷蒗稂",
		L"量两粮良辆亮梁凉谅粱晾靓踉莨椋魉墚",
		L"老劳落络牢捞涝烙姥佬崂唠酪潦痨醪铑铹栳耢",
		L"目模木亩幕母牧莫穆姆墓慕牟牡募睦缪沐暮拇姥钼苜仫毪坶",
		L"了乐勒肋叻鳓嘞仂泐",
		L"类累雷勒泪蕾垒磊擂镭肋羸耒儡嫘缧酹嘞诔檑畾",
		L"随岁虽碎尿隧遂髓穗绥隋邃睢祟濉燧谇眭荽",
		L"列烈劣裂猎冽咧趔洌鬣埒捩躐劦",
		L"冷愣棱楞塄",
		L"领令另零灵龄陵岭凌玲铃菱棱伶羚苓聆翎泠瓴囹绫呤棂蛉酃鲮柃",
		L"俩",
		L"了料疗辽廖聊寥缪僚燎缭撂撩嘹潦镣寮蓼獠钌尥鹩",
		L"流刘六留柳瘤硫溜碌浏榴琉馏遛鎏骝绺镏旒熘鹨锍",
		L"论轮伦仑纶沦抡囵",
		L"率律旅绿虑履吕铝屡氯缕滤侣驴榈闾偻褛捋膂稆",
		L"楼露漏陋娄搂篓喽镂偻瘘髅耧蝼嵝蒌",
		L"贸毛矛冒貌茂茅帽猫髦锚懋袤牦卯铆耄峁瑁蟊茆蝥旄泖昴瞀",
		L"龙隆弄垄笼拢聋陇胧珑窿茏咙砻垅泷栊癃",
		L"农浓弄脓侬哝",
		L"双爽霜孀泷",
		L"术书数属树输束述署朱熟殊蔬舒疏鼠淑叔暑枢墅俞曙抒竖蜀薯梳戍恕孰沭赎庶漱塾倏澍纾姝菽黍腧秫毹殳疋摅",
		L"率衰帅摔甩蟀",
		L"略掠锊",
		L"么马吗摩麻码妈玛嘛骂抹蚂唛蟆犸杩",
		L"么麽",
		L"买卖麦迈脉埋霾荬劢",
		L"满慢曼漫埋蔓瞒蛮鳗馒幔谩螨熳缦镘颟墁鞔嫚",
		L"米密秘迷弥蜜谜觅靡泌眯麋猕谧咪糜宓汨醚嘧弭脒冖幂祢縻蘼芈糸敉沕",
		L"们门闷瞒汶扪焖懑鞔钔",
		L"忙盲茫芒氓莽蟒邙硭漭",
		L"蒙盟梦猛孟萌氓朦锰檬勐懵蟒蜢虻黾蠓艨甍艋瞢礞",
		L"苗秒妙描庙瞄缪渺淼藐缈邈鹋杪眇喵冇",
		L"某谋牟缪眸哞鍪蛑侔厶踎",
		L"缪谬",
		L"美没每煤梅媒枚妹眉魅霉昧媚玫酶镁湄寐莓袂楣糜嵋镅浼猸鹛",
		L"文问闻稳温纹吻蚊雯紊瘟汶韫刎璺玟阌揾",
		L"灭蔑篾乜咩蠛",
		L"明名命鸣铭冥茗溟酩瞑螟暝",
		L"内南那纳拿哪娜钠呐捺衲镎肭乸",
		L"内那哪馁",
		L"难诺挪娜糯懦傩喏搦锘",
		L"若弱偌箬叒",
		L"囊馕囔曩攮",
		L"脑闹恼挠瑙淖孬垴铙桡呶硇猱蛲嫐",
		L"你尼呢泥疑拟逆倪妮腻匿霓溺旎昵坭铌鲵伲怩睨猊妳",
		L"嫩恁",
		L"能",
		L"您恁",
		L"鸟尿溺袅脲茑嬲",
		L"摄聂捏涅镍孽捻蘖啮蹑嗫臬镊颞乜陧聶",
		L"娘酿",
		L"宁凝拧泞柠咛狞佞聍甯寗",
		L"努怒奴弩驽帑孥胬",
		L"女钕衄恧",
		L"入如女乳儒辱汝茹褥孺濡蠕嚅缛溽铷洳薷襦颥蓐",
		L"暖",
		L"虐疟",
		L"热若惹喏",
		L"区欧偶殴呕禺藕讴鸥瓯沤耦怄",
		L"跑炮泡抛刨袍咆疱庖狍匏脬",
		L"剖掊裒",
		L"喷盆湓",
		L"瞥撇苤氕丿潎",
		L"品贫聘频拼拚颦姘嫔榀牝",
		L"色塞瑟涩啬穑铯槭歮",
		L"情青清请亲轻庆倾顷卿晴氢擎氰罄磬蜻箐鲭綮苘黥圊檠謦甠暒凊郬靘",
		L"赞暂攒堑昝簪糌瓒錾趱拶",
		L"少绍召烧稍邵哨韶捎勺梢鞘芍苕劭艄筲杓潲",
		L"扫骚嫂梢缫搔瘙臊埽缲鳋",
		L"沙厦杀纱砂啥莎刹杉傻煞鲨霎嗄痧裟挲铩唼歃",
		L"县选宣券旋悬轩喧玄绚渲璇炫萱癣漩眩暄煊铉楦泫谖痃碹揎镟儇烜翾昍",
		L"然染燃冉苒髯蚺",
		L"让壤攘嚷瓤穰禳",
		L"绕扰饶娆桡荛",
		L"仍扔",
		L"日",
		L"肉柔揉糅鞣蹂",
		L"软阮朊",
		L"润闰",
		L"萨洒撒飒卅仨脎",
		L"所些索缩锁莎梭琐嗦唆唢娑蓑羧挲桫嗍睃惢",
		L"思赛塞腮噻鳃嘥嗮",
		L"说水税谁睡氵",
		L"桑丧嗓搡颡磉",
		L"森",
		L"僧",
		L"筛晒",
		L"上商尚伤赏汤裳墒晌垧觞殇熵绱",
		L"行省星腥猩惺兴刑型形邢饧醒幸杏性姓陉荇荥擤悻硎",
		L"收手受首售授守寿瘦兽狩绶艏扌",
		L"说数硕烁朔铄妁槊蒴搠",
		L"速素苏诉缩塑肃俗宿粟溯酥夙愫簌稣僳谡涑蔌嗉觫甦",
		L"刷耍唰",
		L"栓拴涮闩",
		L"顺瞬舜吮",
		L"送松宋讼颂耸诵嵩淞怂悚崧凇忪竦菘",
		L"艘搜擞嗽嗖叟馊薮飕嗾溲锼螋瞍",
		L"损孙笋荪榫隼狲飧",
		L"腾疼藤滕誊",
		L"铁贴帖餮萜",
		L"土突图途徒涂吐屠兔秃凸荼钍菟堍酴",
		L"外歪崴",
		L"王望往网忘亡旺汪枉妄惘罔辋魍",
		L"翁嗡瓮蓊蕹",
		L"抓挝爪",
		L"样养央阳洋扬杨羊详氧仰秧痒漾疡泱殃恙鸯徉佯怏炀烊鞅蛘玚旸飏",
		L"雄兄熊胸凶匈汹芎",
		L"哟唷",
		L"用永拥勇涌泳庸俑踊佣咏雍甬镛臃邕蛹恿慵壅痈鳙墉饔喁",
		L"杂扎咱砸咋匝咂拶雥",
		L"在再灾载栽仔宰哉崽甾",
		L"造早遭枣噪灶燥糟凿躁藻皂澡蚤唣",
		L"贼",
		L"怎谮",
		L"增曾综赠憎锃甑罾缯",
		L"这",
		L"走邹奏揍诹驺陬楱鄹鲰",
		L"转拽",
		L"尊遵鳟樽撙",
		L"嗲",
		L"耨"
	};

	static inline const std::vector<std::wstring> XiaoHePinyinArr = {
		L"aa", L"ee", L"ai", L"ei", L"xi", L"yi", L"an", L"hj", L"ah", L"ao", L"wa", L"yu", L"nq", L"oo", L"ba", L"pa",
		L"pi",
		L"bi", L"bd", L"bo", L"bw", L"bj", L"pj", L"bb", L"bh", L"ph", L"bg", L"bc", L"bu", L"pu", L"mm", L"po", L"fj",
		L"fu", L"bf", L"fg", L"bm", L"pm", L"vf", L"bn", L"pn",
		L"ho", L"bp", L"mb", L"ff", L"bk", L"gg", L"fh", L"xm", L"fz", L"ca", L"ia", L"cd", L"cj", L"uf", L"cf", L"sj",
		L"ch", L"zh", L"if", L"cc", L"ce", L"ze", L"vd", L"dc",
		L"cg", L"va", L"id", L"ci", L"zi", L"co", L"ij", L"uj", L"vj", L"xb", L"lm", L"ih", L"vh", L"ic", L"vc", L"vz",
		L"ie", L"ju", L"ig", L"rs", L"ug", L"dg", L"vi", L"vg",
		L"th", L"ii", L"ui", L"qi", L"ik", L"to", L"do", L"xt", L"is", L"iz", L"qq", L"xq", L"iu", L"tr", L"vv", L"ir",
		L"vr", L"yr", L"cr", L"il", L"vl", L"iv", L"iy", L"vy",
		L"cu", L"dy", L"qu", L"xu", L"io", L"zu", L"ji", L"cs", L"zs", L"cz", L"cv", L"ww", L"cy", L"zo", L"zr", L"da",
		L"dd", L"td", L"ta", L"dj", L"lu", L"tj", L"rf", L"jp",
		L"yj", L"dh", L"tc", L"tn", L"te", L"de", L"dw", L"di", L"ti", L"tv", L"yz", L"dm", L"tm", L"vu", L"nm", L"dn",
		L"yc", L"dp", L"ue", L"ye", L"xp", L"ve", L"dk", L"dq",
		L"tk", L"ds", L"ts", L"vs", L"dz", L"du", L"dr", L"dv", L"rv", L"yt", L"ty", L"hv", L"wu", L"ya", L"he", L"wo",
		L"en", L"nn", L"er", L"fa", L"qr", L"fw", L"pw", L"pk",
		L"fo", L"hu", L"ga", L"ge", L"ha", L"xx", L"gd", L"hd", L"gj", L"gh", L"jl", L"hh", L"gs", L"hs", L"gl", L"qs",
		L"gc", L"hc", L"li", L"jx", L"lo", L"ke", L"qx", L"gw",
		L"gf", L"hf", L"gz", L"kz", L"gu", L"pd", L"gx", L"tz", L"gk", L"kk", L"gr", L"wj", L"ne", L"gv", L"jy", L"js",
		L"jt", L"gy", L"hy", L"go", L"hw", L"kj", L"hg", L"mo",
		L"pg", L"hz", L"hx", L"hk", L"hr", L"xy", L"hl", L"nd", L"lr", L"qp", L"jm", L"nj", L"qm", L"ql", L"xl", L"jn",
		L"vo", L"qn", L"xn", L"si", L"kd", L"jb", L"qb", L"jk",
		L"yk", L"jq", L"zv", L"jr", L"sr", L"yy", L"qy", L"ka", L"kh", L"kg", L"kc", L"kf", L"yb", L"ks", L"ku", L"kx",
		L"kv", L"kr", L"kl", L"qt", L"ky", L"ko", L"la", L"ld",
		L"lj", L"lb", L"lh", L"ll", L"lc", L"mu", L"le", L"lw", L"sv", L"lp", L"lg", L"lk", L"lx", L"ln", L"lq", L"ly",
		L"lv", L"lz", L"mc", L"ls", L"ns", L"ul", L"uu", L"uk",
		L"lt", L"ma", L"me", L"md", L"mj", L"mi", L"mf", L"mh", L"mg", L"mn", L"mz", L"mq", L"mw", L"wf", L"mp", L"mk",
		L"na", L"nw", L"no", L"ro", L"nh", L"nc", L"ni", L"nf",
		L"ng", L"nb", L"nn", L"np", L"nl", L"nk", L"nu", L"nv", L"ru", L"nr", L"nt", L"re", L"ou", L"pc", L"pz", L"pf",
		L"pp", L"pb", L"se", L"qk", L"zj", L"uc", L"sc", L"ua",
		L"xr", L"rj", L"rh", L"rc", L"rg", L"ri", L"rz", L"rr", L"ry", L"sa", L"so", L"sd", L"uv", L"sh", L"sf", L"sg",
		L"ud", L"uh", L"xk", L"uz", L"uo", L"su", L"ux", L"ur",
		L"uy", L"ss", L"sz", L"sy", L"tg", L"tp", L"tu", L"wd", L"wh", L"wg", L"vx", L"yh", L"xs", L"yo", L"ys", L"za",
		L"zd", L"zc", L"zw", L"zf", L"zg", L"vw", L"zz", L"vk",
		L"zy", L"dx", L"nz"
	};
};
