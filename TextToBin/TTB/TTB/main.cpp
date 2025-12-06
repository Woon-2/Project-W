#include <cstdint>
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <string_view>

struct FrameInfo
{
	std::string name;
	std::uint32_t width = 0;
	std::uint32_t height = 0;
};

enum class AnimType : std::uint32_t
{
	Loop = 0,
	Once = 1,
    RandomAdvance = 2,
    SIZE
};

struct AnimationData
{
	std::string name;
	AnimType type = AnimType::Loop;
	std::uint32_t frameCount = 0;
	std::uint32_t frameTimeMs = 0;
    std::uint32_t durationMs = 0;
	std::vector<FrameInfo> frames;
};

// uint32_t 하나 쓰기
void WriteUInt32( std::ofstream& out, std::uint32_t value )
{
    out.write( reinterpret_cast<const char*>(&value), sizeof( value ) );
}

void WriteU8String( std::ofstream& out, std::string_view s )
{
    if ( s.size() > 255 ) { /* 에러 처리 */ }
    uint8_t len = static_cast<uint8_t>(s.size());
    out.write( reinterpret_cast<const char*>(&len), 1 );
    out.write( s.data(), len );
}

// length + 문자열 쓰기 (null 없이)
void WriteString( std::ofstream& out, std::string_view str )
{
    if ( str.size() > 255 ) {
        // 여기서 에러 처리/예외/로그 등
        std::cerr << "[WriteStringU8] string too long: " << str.size() << "\n";
        std::exit( -1 );
    }

    std::uint8_t len = static_cast<std::uint8_t>(str.size());
    out.write( reinterpret_cast<const char*>(&len), sizeof( len ) );
    if ( len > 0 ) out.write( str.data(), len );
}

inline void WriteHeadTag( std::ofstream& out, std::string_view tagSource )
{
    // "<Name>" 같은 열린 태그를 length + 문자열로 기록
    std::string tag = "<";
    tag += tagSource;
    tag += ":>";
    WriteU8String( out, tag );
}

inline void WriteTailTag( std::ofstream& out, std::string_view tagSource )
{
    // "</Name>" 같은 닫힌 태그를 length + 문자열로 기록
    std::string tag = "</";
    tag += tagSource;
    tag += ">";
    WriteU8String( out, tag );
}

// C#의 WriteText와 동일한 역할
inline void WriteText( std::ofstream& out,
    std::string_view tagSource,
    std::string_view text )
{
    WriteHeadTag( out, tagSource );
    WriteString( out, std::string( text ) );   // string_view → string
    WriteTailTag( out, tagSource );
}

inline void WriteTextUInt32( std::ofstream& out, std::string_view tag, uint32_t v ) {
    WriteHeadTag( out, tag );
    out.write( reinterpret_cast<const char*>(&v), 4 );
    WriteTailTag( out, tag );
}

bool ExportAnimationBinary( const AnimationData& anim, const std::string& outputPath )
{
    std::ofstream out( outputPath, std::ios::binary );
    if ( !out )
    {
        std::cerr << "파일을 열 수 없습니다: " << outputPath << "\n";
        return false;
    }

    // [Animation Info]
	WriteText( out, "Name", anim.name );

    // Type
    WriteTextUInt32( out, "Type", static_cast<uint32_t>(anim.type) );

    // FrameCnt, FrameTime, Duration
    WriteTextUInt32( out, "FrameCount", anim.frameCount );
    WriteTextUInt32( out, "FrameTime", anim.frameTimeMs );
    WriteTextUInt32( out, "Duration", anim.durationMs );

    // [Frames]
	WriteString( out, "<Frames:>" ); // Frames 시작 태그
    for ( const auto& frame : anim.frames )
    {
		WriteString( out, "<Frame:>" ); // Frame 시작 태그
        WriteText( out, "Name", frame.name );
        WriteTextUInt32( out, "Width", frame.width  );
        WriteTextUInt32( out, "Height",  frame.height );
		WriteString( out, "</Frame>" ); // Frame 종료 태그
    }
	WriteString( out, "</Frames>" ); // Frames 종료 태그

    return true;
}

// ----- 추가: 바이너리 덤프 함수 -----
void DumpBinaryFileRaw_U8( const std::string& path )
{
    std::ifstream in( path, std::ios::binary );
    if ( !in ) return;

    std::size_t index = 0;
    while ( true )
    {
        uint8_t len = 0;
        if ( !in.read( reinterpret_cast<char*>(&len), 1 ) ) break;

        std::string data;
        data.resize( len );
        if ( len > 0 && !in.read( data.data(), len ) ) break;

        std::cout << index++ << ": [len(u8)=" << (int)len << "] \"" << data << "\"\n";
    }
}

int main()
{
    AnimationData anim;

    // --- 기본 정보 입력 ---
    std::cout << "애니메이션 이름을 입력하세요: ";
    std::getline( std::cin, anim.name );

    int typeInput = 0;
    std::cout << "타입을 선택하세요 (0: Loop, 1: Once, 2: RandomAdvance): ";
    std::cin >> typeInput;
    anim.type = static_cast<AnimType>(typeInput);

    std::cout << "프레임 개수(FrameCnt)를 입력하세요: ";
    std::cin >> anim.frameCount;

    std::cout << "프레임 재생 시간(ms) (FrameTime)을 입력하세요: ";
    std::cin >> anim.frameTimeMs;

    std::cout << "총 재생 시간(ms) (duration)을 입력하세요: ";
    std::cin >> anim.durationMs;

    anim.frames.resize( anim.frameCount );

	std::cout << "각 스프라이트의 크기가 동일한가요? (y/n): ";
	char sameSizeInput;
	std::cin >> sameSizeInput;
	bool sameSize = (sameSizeInput == 'y' || sameSizeInput == 'Y');

    // 개행 문자 처리
    std::cin.ignore( std::numeric_limits<std::streamsize>::max(), '\n' );

    if( sameSize )
    {
        std::uint32_t commonWidth = 0;
        std::uint32_t commonHeight = 0;
        std::cout << "공통 Width를 입력하세요: ";
        std::cin >> commonWidth;
        std::cout << "공통 Height를 입력하세요: ";
        std::cin >> commonHeight;
        // 개행 문자 처리
        std::cin.ignore( std::numeric_limits<std::streamsize>::max(), '\n' );

		// --- 각 프레임 정보 입력 ---
        // 애니메이션 이름으로 프레임 이름 자동 생성
        for ( std::uint32_t i = 0; i < anim.frameCount; ++i )
        {
            FrameInfo& f = anim.frames[i];
			f.name = anim.name + "_" + std::to_string( i ) + ".dds";
            f.width = commonWidth;
            f.height = commonHeight;
            std::cout << "==== Frame " << i << " ====\n";
            std::cout << "DDS 파일 이름: " << f.name << "\n";
            std::cout << "Width: " << f.width << "\n";
			std::cout << "Height: " << f.height << "\n";
		}
	}
    else 
    {
        // --- 각 프레임 정보 입력 ---
        for ( std::uint32_t i = 0; i < anim.frameCount; ++i )
        {
            FrameInfo& f = anim.frames[i];

            std::cout << "==== Frame " << i << " ====\n";

            std::cout << "DDS 파일 이름 (예: Slime_" << i << ".dds): ";
            std::getline( std::cin, f.name );

            std::cout << "Width: ";
            std::cin >> f.width;

            std::cout << "Height: ";
            std::cin >> f.height;

            std::cin.ignore( std::numeric_limits<std::streamsize>::max(), '\n' );
        }
    }    

    // --- 출력 파일 이름 입력 ---
    std::string outputPath;
    std::cout << "저장할 바이너리 파일 이름을 입력하세요 (예: slime_anim.bin): ";
    std::getline( std::cin, outputPath );

    if ( ExportAnimationBinary( anim, outputPath ) )
    {
        std::cout << "저장에 성공했습니다: " << outputPath << "\n";

        // (191번째 줄 요구) 바이너리 파일 원시 구조 덤프
        DumpBinaryFileRaw_U8( outputPath );
    }
    else
    {
        std::cout << "저장에 실패했습니다.\n";
    }

    return 0;
}
