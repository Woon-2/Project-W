#include <cstdint>
#include <iostream>
#include <string>
#include <fstream>
#include <vector>

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
    SIZE
};

struct AnimationData
{
	std::string name;
	AnimType type = AnimType::Loop;
	std::uint32_t frameCount = 0;
	std::uint32_t frameTimeMs = 0;
	std::vector<FrameInfo> frames;
};

// uint32_t 하나 쓰기
void WriteUInt32( std::ofstream& out, std::uint32_t value )
{
    out.write( reinterpret_cast<const char*>(&value), sizeof( value ) );
}

// length + 문자열 쓰기 (null 없이)
void WriteString( std::ofstream& out, const std::string& str )
{
    std::uint32_t len = static_cast<std::uint32_t>(str.size());
    WriteUInt32( out, len );
    if ( len > 0 )
        out.write( str.data(), len );
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
    WriteString( out, anim.name );                      // Name

    // Type
    WriteUInt32( out, static_cast<std::uint32_t>(anim.type) );

    // FrameCnt, FrameTime
    WriteUInt32( out, anim.frameCount );
    WriteUInt32( out, anim.frameTimeMs );

    // [Frames]
    for ( const auto& frame : anim.frames )
    {
        WriteString( out, frame.name );                 // Name
        WriteUInt32( out, frame.width );               // Width
        WriteUInt32( out, frame.height );              // Height
    }

    return true;
}

int main()
{
    AnimationData anim;

    // --- 기본 정보 입력 ---
    std::cout << "애니메이션 이름을 입력하세요: ";
    std::getline( std::cin, anim.name );

    int typeInput = 0;
    std::cout << "타입을 선택하세요 (0: Loop, 1: Once): ";
    std::cin >> typeInput;
    anim.type = (typeInput == 1) ? AnimType::Once : AnimType::Loop;

    std::cout << "프레임 개수(FrameCnt)를 입력하세요: ";
    std::cin >> anim.frameCount;

    std::cout << "프레임 재생 시간(ms) (FrameTime)을 입력하세요: ";
    std::cin >> anim.frameTimeMs;

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
    }
    else
    {
        std::cout << "저장에 실패했습니다.\n";
    }

    return 0;
}
