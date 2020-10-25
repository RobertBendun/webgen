#include <iostream>
#include <filesystem>
#include <string_view>
#include <fstream>
#include <string>
#include <numeric>
#include <optional>
#include <regex>
#include <iterator>

namespace fs = std::filesystem;
using namespace std::string_view_literals;

auto read_file(fs::path path) -> std::string
{
	auto file = std::ifstream(path, std::ios::in | std::ios::binary);
	auto const file_size = fs::file_size(path);
	auto result = std::string(file_size, '\0');
	file.read(result.data(), file_size);
	return result;
}

auto try_template_extraction(std::string const& file) -> std::optional<std::string>
{
	auto match_results = std::smatch{};
	static auto template_regex = std::regex(R"(<!--\s*template:\s*([a-z0-9A-Z._-]+)\s*-->)");
	if (!std::regex_search(file, match_results, template_regex) || match_results.empty()) {
		return std::nullopt;
	}
	return match_results.str(1);
}

auto mix_article_and_template_into_file(fs::path const& output_path, fs::path const& template_path, std::string &&article)
{
	auto out = std::ofstream(output_path, std::ios::out | std::ios::trunc | std::ios::binary);
	static auto template_regex = std::regex(R"(<!--\s*insert\s*body\s*-->)", std::regex_constants::icase);
	auto out_it = std::ostreambuf_iterator<char>(out);
	auto temp = read_file(template_path);
	std::regex_replace(out_it, temp.begin(), temp.end(), template_regex, article);
}

auto main() -> int
try {
	fs::current_path("C:\\home\\dev\\website\\");

	auto templates = fs::path("templates");

	if (!fs::exists(templates) || !fs::is_directory(templates)) {
		std::cerr << "Directory " << templates.string() << " does not exists. Nothing to do.";
		return 1;
	}

	if (fs::exists("build")) {
		std::cout << "Delete build directory? [y/n]: ";
		std::string response;
		std::getline(std::cin, response);
		if (response.size() == 0 || (response[0] != 'y' && response[0] != 'Y'))
			return 2;
		fs::remove_all("build");
	}

	fs::create_directory("build");
	if (fs::exists("public")) {
		std::cout << "Copying public files\n";
		fs::copy("public/", "build/", fs::copy_options::recursive | fs::copy_options::overwrite_existing);
	}

	auto count_of_articles = std::accumulate(fs::recursive_directory_iterator("articles"), {}, 0u,
		[](auto p, fs::path const& entry) {
			return (fs::is_directory(entry) ? 0 : 1) + p;
		});


	auto article_index = 1u;
	for (auto const& entry : fs::recursive_directory_iterator("articles")) {
		if (entry.is_directory())
			continue;

		auto const& path = entry.path();
		std::cout << "Article [" << article_index << "/" << count_of_articles << "]: " << path.string() << '\n';

		auto article = read_file(path);
		auto template_path = templates / try_template_extraction(article).value_or("default.html");
		if (!fs::exists(template_path)) {
			std::cerr << "Could not find template " << template_path.string() << " referenced by " << path.string() << '\n';
			return 3;
		}

		auto output = "build" / fs::relative(entry, "articles");
		auto output_dirs = output.parent_path();
		if (!fs::exists(output_dirs)) {
			std::cout << "Creating " << output_dirs.string() << " directories\n";
			fs::create_directories(output_dirs);
		}

		mix_article_and_template_into_file(output, template_path, std::move(article));
		article_index += 1;
	}
} catch (std::exception const& e) {
	std::cerr << "\n------------- EXCEPTION -------------\n" << e.what() << '\n';
	return 424242;
}
