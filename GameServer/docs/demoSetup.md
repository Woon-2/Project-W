# 시연 PC 셋업 체크리스트

작성: 2026-07-26

새 PC에서 프로젝트를 처음 돌릴 때 필요한 전 과정. **DB는 코드를 따라가지 않는다** —
LocalDB를 설치해도 `ProjectW` 데이터베이스와 테이블은 없고, `db/schema.sql`을 직접 적용해야 한다.

빠른 요약:

```
1. VS 18 + LocalDB + ODBC Driver 17 설치
2. 저장소 체크아웃
3. ServerEngine Rebuild → LobbyServer / RoomServer / client 빌드   (client 필수: 런타임 DLL 복사)
4. sqlcmd -S "(localdb)\MSSQLLocalDB" -i db\schema.sql
5. LobbyServer → RoomServer → client 실행 (작업 디렉터리 = 각 프로젝트 폴더)
6. 시연 계정 새로 가입 (계정 데이터는 PC를 따라가지 않는다)
```

---

## 1. 소프트웨어 설치

### Visual Studio 18

PlatformToolset이 **v145**라 VS 18이 필요하다. VS 2022(v143)로는 빌드되지 않는다.
워크로드: **C++를 사용한 데스크톱 개발**.
구성은 **x64만** 쓴다 (`x86`은 `Win32`에 매핑되며 아무것도 만들지 않는다).

### SQL Server Express LocalDB

VS Installer → 개별 구성 요소 → **SQL Server Express LocalDB**.
(또는 "데이터 저장 및 처리" 워크로드에 포함된다.)

### ODBC Driver 17 for SQL Server ← **가장 흔한 사고 지점**

`db_config.json`이 드라이버를 **이름으로** 지정한다:

```
Driver={ODBC Driver 17 for SQL Server};Server=(localdb)\MSSQLLocalDB;Database=ProjectW;Trusted_Connection=Yes;
```

**LocalDB를 설치해도 Driver 17이 반드시 함께 깔린다는 보장이 없다.** 이름이 안 맞으면
서버가 `SQLSTATE IM002`(데이터 원본 이름이 없음)를 출력하고 리스너를 띄우기 전에 종료한다.

확인:

```powershell
Get-OdbcDriver -Platform 64-bit | Where-Object Name -like "*SQL Server*" | Select-Object -ExpandProperty Name
```

`ODBC Driver 17 for SQL Server`가 나오면 통과. 없고 **18만 있다면** 두 가지 중 하나:

- Microsoft에서 ODBC Driver 17을 따로 설치한다 (권장 — 설정 파일을 안 건드린다), 또는
- `db_config.json`의 연결 문자열을 18용으로 바꾼다. **18은 기본이 `Encrypt=yes`**라
  LocalDB의 자체 서명 인증서를 신뢰하도록 한 항목을 더 붙여야 한다:

  ```
  Driver={ODBC Driver 18 for SQL Server};Server=(localdb)\MSSQLLocalDB;Database=ProjectW;Trusted_Connection=Yes;TrustServerCertificate=Yes;
  ```

### sqlcmd

보통 Client SDK에 딸려 온다(`...\Microsoft SQL Server\Client SDK\ODBC\170\Tools\Binn\SQLCMD.EXE`).
`sqlcmd -?`로 확인. 없으면 VS의 **SQL Server 개체 탐색기**에서 스크립트를 열어 실행해도 된다.

> 참고 — 이 문서를 쓴 개발 PC의 구성: SQL Server 2025 LocalDB 17.0.4025.3,
> ODBC Driver 17 for SQL Server 17.10.6.1, Command Line Utilities 15.

---

## 2. 저장소 체크아웃

설정 파일 3개는 저장소에 커밋돼 있어 그대로 따라온다. 수정할 필요 없다(2번 항목 예외).

| 파일 | 용도 |
|---|---|
| `db_config.json` | ODBC 연결 문자열, 커넥션 풀 크기 |
| `security_config.json` | 입장 티켓 서명 키. **로비/룸 서버가 같은 값을 봐야 한다** |
| `network_config.json` | 로비 `127.0.0.1:8888`, 룸 `127.0.0.1:9000` |

**`x64/`는 gitignore 대상이라 클론에 실행 파일도 런타임 DLL도 없다.** 반드시 빌드해야 한다.

---

## 3. 빌드

**`ServerEngine`을 항상 먼저, 그리고 Rebuild로.** `lib\<Config>\ServerEngine.lib`는 저장소에
커밋된 바이너리라, 체크아웃으로 파일 mtime이 obj보다 최신이 되면 MSBuild가 링크를 건너뛰고
낡은 lib을 남긴다. 그 상태로 서버를 빌드하면 LNK2019가 난다.

```powershell
$msbuild = "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe"
& $msbuild GameServer.sln /t:ServerEngine:Rebuild /p:Configuration=Debug /p:Platform=x64
& $msbuild GameServer.sln /t:LobbyServer`;RoomServer`;client /p:Configuration=Debug /p:Platform=x64
```

VS에서 할 때도 순서는 같다 — ServerEngine 리빌드 후 나머지.

### client를 최소 한 번은 빌드할 것

`client`의 PostBuildEvent(`client/client.vcxproj`)가 런타임 DLL 3개를 출력 폴더로 복사한다:

| DLL | 출처 | 필요한 쪽 |
|---|---|---|
| `dxcompiler.dll` | `$(WindowsSdkDir)Redist\D3D\x64` | client |
| `dxil.dll` | 〃 | client |
| `lua54.dll` | `$(SolutionDir)` (저장소에 커밋돼 있음) | **RoomServer**, client |

즉 **서버만 빌드하면 `x64\<Config>\`에 `lua54.dll`이 없어 RoomServer가 기동하지 못한다.**
서버만 쓸 계획이어도 client를 한 번 빌드하거나, 루트의 `lua54.dll`을 출력 폴더로 복사한다.

---

## 4. DB 준비

```powershell
sqllocaldb start MSSQLLocalDB
sqlcmd -S "(localdb)\MSSQLLocalDB" -i db\schema.sql
```

`schema.sql`은 **멱등**이다 — 반복 실행해도 안전하다.

적용 확인:

```powershell
sqlcmd -S "(localdb)\MSSQLLocalDB" -d ProjectW -Q "SET NOCOUNT ON; SELECT t.name, COUNT(c.column_id) FROM sys.tables t JOIN sys.columns c ON c.object_id=t.object_id GROUP BY t.name;" -h -1 -W
```

`Account 6` / `Inventory 4`가 나오면 정상이다.

> LocalDB는 정지 상태여도 연결 시도가 인스턴스를 자동 기동시키므로 `sqllocaldb start`는
> 사실상 확인용이다. 인스턴스 목록은 `sqllocaldb info`로 본다.

---

## 5. 실행

**작업 디렉터리는 반드시 각 프로젝트 폴더여야 한다.** 서버와 클라가 에셋을 `../resources/...`
상대경로로 찾기 때문이다. 어느 프로젝트에도 `LocalDebuggerWorkingDirectory`가 설정돼 있지 않아
**VS의 기본값 `$(ProjectDir)`에 의존**하고 있다 — VS에서 실행하면 정상이지만, `x64\Debug\`의
exe를 탐색기나 명령줄에서 직접 실행하면 모델·애니메이션·레벨을 전부 못 찾고 RoomServer가 죽는다.

명령줄에서 띄운다면:

```powershell
Start-Process ".\x64\Debug\LobbyServer.exe" -WorkingDirectory ".\LobbyServer"
Start-Process ".\x64\Debug\RoomServer.exe"  -WorkingDirectory ".\RoomServer"
```

순서는 **LobbyServer → RoomServer → client**.

### 기동 확인은 로그가 아니라 포트로

```powershell
Get-NetTCPConnection -State Listen -LocalPort 8888,9000
```

둘 다 뜨면 DB 연결과 보안 설정 로드를 **모두 통과했다는 뜻**이다 — 두 서버 모두 그 초기화에
실패하면 리스너를 띄우기 전에 종료한다. 콘솔 출력은 파일로 리다이렉트하면 버퍼링돼 한참 뒤에야
보이므로, 로그를 봐야 할 때는 콘솔 창에서 직접 실행할 것.

---

## 6. 시연 계정

**계정과 인벤토리는 PC를 따라가지 않는다.** 새 PC의 `dbo.Account`는 비어 있으므로 클라이언트에서
회원가입부터 해야 한다. 신규 계정은 첫 입장 때 스타터 포션 5개를 받으므로 시연에는 오히려 유리하다.

아이템 획득 경로(루팅)가 아직 없어 **포션은 줄어들기만 한다.** 반복 시연으로 바닥나면 초기화한다:

```sql
DELETE FROM dbo.Inventory WHERE accountId = <id>;   -- 다음 입장 때 신규 취급 → 5개 재지급
```

계정 목록 확인:

```powershell
sqlcmd -S "(localdb)\MSSQLLocalDB" -d ProjectW -Q "SET NOCOUNT ON; SELECT accountId, loginId, nickname FROM dbo.Account;" -h -1 -W
```

---

## 7. 여러 PC로 나눌 때

**LocalDB는 네트워크 접속을 지원하지 않는다** — 같은 PC의 로컬 프로세스만 붙는다.
따라서 **로비 서버와 룸 서버는 반드시 같은 PC**에 있어야 한다. 서버를 물리적으로 분리하려면
SQL Server Express(TCP 활성화)로 옮겨야 한다.

클라이언트를 다른 PC에서 돌리려면:

1. `network_config.json`의 `ip`를 `127.0.0.1` → 서버 PC의 LAN IP로 바꾼다.
   **두 항목(lobby, room) 모두** 바꿔야 한다 — 룸 주소는 로비가 `S_GameStart`로 클라에 알려주므로
   서버 쪽 값이 곧 클라가 접속할 주소가 된다.
2. 서버 PC 방화벽에서 TCP **8888**, **9000**을 인바운드 허용한다.
3. 클라 PC에도 같은 `network_config.json`이 있어야 한다(로비 주소를 클라가 직접 읽는다).

`security_config.json`은 **서버 전용**이다. 클라 PC에 복사하지 말 것 — 서명 키가 유출되면
누구나 입장 티켓을 위조할 수 있다.

---

## 8. 증상 → 원인

| 증상 | 원인과 조치 |
|---|---|
| 로그인 화면이 안 나오고 바로 게임이 시작된다 | 로비 서버 접속 실패 → **StandAlone 폴백**(`client/main.cpp:143-146`). 서버가 떠 있는지, `network_config.json`의 주소가 맞는지 확인 |
| 서버가 켜자마자 꺼지고 `SQLSTATE IM002` | ODBC Driver 이름 불일치 → 1번 항목 |
| 서버가 켜자마자 꺼지고 `[DbConfig] failed to connect database` | LocalDB 미설치/미기동, 또는 `ProjectW` DB 없음 → 4번 항목 |
| 서버가 꺼지고 `[SecurityConfig] ...` | `security_config.json`이 exe 상위 경로에 없다. 저장소 밖으로 exe만 옮기지 말 것 |
| RoomServer가 `파일을 열 수 없습니다` 로그를 쏟고 종료 | 작업 디렉터리가 잘못됨 → 5번 항목 |
| RoomServer가 `lua54.dll` 없다며 실행 안 됨 | client를 빌드하지 않았다 → 3번 항목 |
| 빌드 시 LNK2019 (EntryTicket/HmacSha256 등) | 낡은 `ServerEngine.lib` → `/t:ServerEngine:Rebuild` |
| 게임 시작을 눌렀는데 입장하자마자 끊긴다 | 로비/룸의 `security_config.json` 값이 다르거나, 두 서버의 시계가 크게 어긋남. RoomServer 콘솔의 `[EntryTicket] 거부(...)` 사유 확인 |
| 인벤토리가 계속 5개로 돌아온다 | 저장 실패. RoomServer 콘솔의 `[Inventory] 저장 실패` 확인 |

---

## 관련 문서

- 루트 `CLAUDE.md` — 빌드 시스템, 프로토콜, 동시성 모델
- `ServerEngine/docs/accountSystem.md` — 가입/로그인
- `ServerEngine/docs/entryTicket.md` — 입장 티켓, `security_config.json`, 키 교체
- `ServerEngine/docs/databaseLayer.md` — ODBC 래퍼, DBMS 전제
- `RoomServer/docs/inventoryPersistence.md` — 인벤토리 저장/복원
