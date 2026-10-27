"""Verify direct-source filtering and parsing without network dependencies."""
from pathlib import Path
import subprocess

def test_direct_source_parsing(tmp_path):
 source=tmp_path/'quick_sources_test.cpp'
 source.write_text('#include "quick_sources.hpp"\n#include <iostream>\nint main(){using namespace muse_quick;J m={{"items",J::array({{{"title","File:Town.jpg"},{"type","image"},{"showInGallery",true},{"srcset",J::array({{{"src","//thumb.wikimedia.org/wikipedia/commons/thumb/a/aa/Town.jpg/500px-Town.jpg?utm=x"}}})}},{{"title","File:Flag.svg"},{"type","image"},{"showInGallery",true},{"srcset",J::array({{{"src","//thumb.wikimedia.org/flag.svg"}}})}}})}};auto s=slides(m,"Town","https://en.wikipedia.org/wiki/Town");if(s.size()!=1||s[0]["url"]!="https://thumb.wikimedia.org/wikipedia/commons/thumb/a/aa/Town.jpg/500px-Town.jpg")return 1;if(topic("muse \'show me stuff on beijing\'")!="beijing")return 2;if(article(topic("show me stuff on the yankees"))!="New York Yankees")return 3;if(encode("New York")!="New%20York")return 4;auto v=video(youtube_data("var ytInitialData = {\\"x\\":[{\\"videoRenderer\\":{\\"videoId\\":\\"abcdefghijk\\",\\"lengthText\\":{},\\"title\\":{\\"runs\\":[{\\"text\\":\\"A } tricky title\\"}]}}}]}; trailing"));if(v["url"]!="https://www.youtube.com/watch?v=abcdefghijk"||!v["stream"].get<bool>())return 5;std::cout<<"Quick source parser checks passed\\n";}\n')
 binary=tmp_path/'quick_sources_test'
 subprocess.run(['g++','-std=c++17','-O0','-I',str(Path(__file__).parent),str(source),'-o',str(binary)],check=True)
 assert 'checks passed' in subprocess.check_output([str(binary)],text=True)


def test_first_presentation_is_retained_across_echoes_updates_and_restart(tmp_path):
 source=tmp_path/'policy.cpp'
 source.write_text(r'''#include "presentation_policy.hpp"
 #include <cassert>
 int main(){muse_ui::PresentationPolicy p;
 assert(p.begin("show me stuff on japan","request-1",true));
 assert(!p.begin("muse 'show me stuff on JAPAN'","echo"));
 assert(p.token=="request-1");
 assert(p.retain("genui.present")&&p.retain("media.ui")&&p.retain("genui.fetch"));
 assert(!p.retain("genui.status")&&!p.retain("audio.radio"));
 assert(!p.retain("media.show",{{"kind","audio"}}));
 p.published=true;muse_ui::PresentationPolicy restored;restored.restore(p.state());
 assert(restored.retain("genui.present"));
 assert(!restored.begin("Japan","late-fetch"));
 assert(restored.begin("show me stuff on Germany","request-2",true));
 assert(restored.token=="request-2"&&!restored.published);
 assert(restored.begin("show me stuff on Germany","request-3",true));
 assert(muse_ui::PresentationPolicy::key("USA")==muse_ui::PresentationPolicy::key("United States"));
 }''')
 binary=tmp_path/'policy'
 subprocess.run(['g++','-std=c++17','-O0','-I',str(Path(__file__).parent),str(source),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
