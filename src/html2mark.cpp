#include "html2mark.h"
#include "xmlreader.h"
#include <chthon2/log.h>
#include <chthon2/util.h>
#include <sstream>
#include <vector>
#include <cctype>

namespace Html2Mark {

std::string collapse(const std::string & data,
		bool remove_heading, bool remove_trailing)
{
	std::string result;
	bool is_space_group = false;
	for(char c : data) {
		if(isspace(c)) {
			if(!is_space_group) {
				result += ' ';
				is_space_group = true;
			}
		} else {
			result += c;
			is_space_group = false;
		}
	}
	if(remove_heading && !result.empty() && result[0] == ' ') {
		result.erase(0, 1);
	}
	if(remove_trailing && !result.empty() && result[result.size() - 1] == ' ') {
		result.erase(result.size() - 1, 1);
	}
	return result;
}

struct TaggedContent {
	std::string tag, content;
	TaggedContent(const std::string & given_tag = std::string(),
			const std::string & given_content = std::string())
		: tag(given_tag), content(given_content)
	{}
	std::string process_tag() const;
};

std::string TaggedContent::process_tag() const
{
	if(tag == "p") {
		return "\n" + content + "\n";
	}
	if(tag == "") {
		return content;
	}
	return Chthon::format("<{0}>{1}</{0}>", tag, content);
}

struct Html2MarkProcessor {
	Html2MarkProcessor(const std::string & html, int html_options,
			int html_min_reference_links_length);
	void process();
	const std::string & get_result() const { return result; }
private:
	std::istringstream stream;
	int options;
	int min_reference_links_length;
	std::string result;
	std::vector<TaggedContent> parts;

	std::string process_tag(const TaggedContent & value);
	void collapse_tag(const std::string & tag = std::string());
};

Html2MarkProcessor::Html2MarkProcessor(const std::string & html, int html_options,
		int html_min_reference_links_length)
	: stream(html), options(html_options),
	min_reference_links_length(html_min_reference_links_length)
{}

std::string Html2MarkProcessor::process_tag(const TaggedContent & value)
{
	if(value.tag.empty()) {
		return value.content;
	} else if(value.tag == "p") {
		return "\n" + collapse(value.content, true, true) + "\n";
	} else if(value.tag == "em") {
		return value.content.empty() ? "" : "_" + value.content + "_";
	} else if(value.tag == "i") {
		return value.content.empty() ? "" : "_" + value.content + "_";
	} else if(value.tag == "b") {
		return value.content.empty() ? "" : "**" + value.content + "**";
	} else if(value.tag == "strong") {
		return value.content.empty() ? "" : "**" + value.content + "**";
	} else if(value.tag == "code") {
		return value.content.empty() ? "" : "`" + value.content + "`";
	} else if(Chthon::starts_with(value.tag, "h")) {
		if(value.content.empty()) {
			return "";
		}
		size_t level = strtoul(value.tag.substr(1).c_str(), nullptr, 10);
		if(level < 1 || 6 < level) {
			return Chthon::format("<{0}>{1}</{0}>", value.tag, value.content);
		}
		std::string content = collapse(value.content, true, true);
		if(level <= 2 && options & UNDERSCORED_HEADINGS) {
			char underscore = level == 1 ? '=' : '-';
			return "\n" + content + "\n" +
				std::string(content.size(), underscore) + "\n";
		}
		return "\n" + std::string(level, '#') +  " " + content + "\n";
	}
	return Chthon::format("<{0}>{1}</{0}>", value.tag, value.content);
}

void Html2MarkProcessor::collapse_tag(const std::string & tag)
{
	while(!parts.empty()) {
		TaggedContent value = parts.back();
		parts.pop_back();
		if(parts.empty()) {
			result += process_tag(value);
		} else {
			parts.back().content += process_tag(value);
		}
		if(value.tag == tag) {
			break;
		}
	}
}

void Html2MarkProcessor::process()
{
	XMLReader reader(stream);

	std::string tag = reader.to_next_tag();
	std::string content = collapse(reader.get_current_content());
	result += content;
	while(!tag.empty()) {
		reader.to_next_tag();
		content = reader.get_current_content();

		if(Chthon::starts_with(tag, "/")) {
			std::string open_tag = tag.substr(1);
			bool found = parts.rend() != std::find_if(
					parts.rend(), parts.rbegin(),
					[&open_tag](const TaggedContent & value) {
						return value.tag == open_tag;
					}
					);
			if(found) {
				collapse_tag(open_tag);
			}
			if(parts.empty()) {
				result += content;
			} else {
				parts.back().content += content;
			}
		} else {
			if(tag == "p") {
				collapse_tag();
				parts.emplace_back(tag, content);
			} else if(Chthon::starts_with(tag, "hr")) {
				result += "\n* * *\n";
				if(parts.empty()) {
					result += content;
				} else {
					parts.back().content += content;
				}
			} else if(Chthon::starts_with(tag, "br")) {
				result += "\n";
				if(parts.empty()) {
					result += content;
				} else {
					parts.back().content += content;
				}
			} else {
				parts.emplace_back(tag, content);
			}
		}

		tag = reader.get_current_tag();
	}
	collapse_tag();
}

std::string html2mark(const std::string & html, int options,
		int min_reference_links_length)
{
	Html2MarkProcessor processor(html, options, min_reference_links_length);
	processor.process();
	return processor.get_result();
}

}