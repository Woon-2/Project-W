-- Project-W 게임 서버 스키마.
-- 멱등 스크립트: 반복 실행해도 안전하다.
-- 적용: sqlcmd -S "(localdb)\MSSQLLocalDB" -i db\schema.sql
-- 문자열 컬럼 크기는 protocol.hpp의 kLoginIdMax/kNicknameMax(널 포함)에서 널을 뺀 값과 일치해야 한다.
-- Inventory의 슬롯 수는 resources/data/inventory.json의 slotCount를 따른다(스키마엔 강제되지 않음).

IF DB_ID(N'ProjectW') IS NULL
    CREATE DATABASE ProjectW;
GO

USE ProjectW;
GO

IF OBJECT_ID(N'dbo.Account', N'U') IS NULL
CREATE TABLE dbo.Account (
    accountId  BIGINT        IDENTITY(1,1) PRIMARY KEY,
    loginId    NVARCHAR(23)  NOT NULL UNIQUE,    -- kLoginIdMax(24) - 1
    pwHash     BINARY(32)    NOT NULL,           -- SHA-256(salt || password)
    pwSalt     BINARY(16)    NOT NULL,
    nickname   NVARCHAR(15)  NOT NULL CONSTRAINT UQ_Account_nickname UNIQUE,   -- kNicknameMax(16) - 1
    createdAt  DATETIME2     NOT NULL DEFAULT SYSUTCDATETIME()
);
GO

-- 마이그레이션: 제약 없이 만들어진 기존 테이블에 닉네임 UNIQUE 추가.
-- 스모크 테스트가 남긴 동일 닉네임 행들을 먼저 정리해야 제약이 걸린다.
IF NOT EXISTS (SELECT 1 FROM sys.key_constraints WHERE name = N'UQ_Account_nickname')
BEGIN
    DELETE FROM dbo.Account WHERE loginId LIKE N'smk%';   -- 테스트 계정
    ALTER TABLE dbo.Account ADD CONSTRAINT UQ_Account_nickname UNIQUE (nickname);
END
GO

-- 룸서버 인벤토리 영속화. 고정 슬롯 모델(resources/data/inventory.json의 slotCount)을 그대로 반영한다.
-- 슬롯 인덱스가 PK인 이유: 같은 itemId가 maxStack을 넘으면 여러 슬롯에 나뉘므로
-- (common/inventory.cpp의 Inventory::add) accountId+itemId를 PK로 두면 그 상태를 저장할 수 없다.
-- 빈 슬롯도 itemId=0, itemCount=0으로 저장한다 → 행이 0개면 "신규 계정",
-- 행이 있는데 전부 비었으면 "다 써서 텅 빈 인벤토리"로 구분된다.
IF OBJECT_ID(N'dbo.Inventory', N'U') IS NULL
CREATE TABLE dbo.Inventory (
    accountId  BIGINT  NOT NULL REFERENCES dbo.Account(accountId),
    slotIndex  INT     NOT NULL,
    itemId     INT     NOT NULL,
    itemCount  INT     NOT NULL,
    PRIMARY KEY (accountId, slotIndex)
);
GO

-- 마이그레이션: slotIndex 없이 선반영만 돼 있던 기존 테이블 교체.
-- 한 번도 읽거나 쓰인 적이 없는 테이블이므로 데이터 보존 없이 재생성한다.
IF EXISTS (SELECT 1 FROM sys.columns
           WHERE object_id = OBJECT_ID(N'dbo.Inventory') AND name = N'itemId')
   AND NOT EXISTS (SELECT 1 FROM sys.columns
           WHERE object_id = OBJECT_ID(N'dbo.Inventory') AND name = N'slotIndex')
BEGIN
    DROP TABLE dbo.Inventory;
    CREATE TABLE dbo.Inventory (
        accountId  BIGINT  NOT NULL REFERENCES dbo.Account(accountId),
        slotIndex  INT     NOT NULL,
        itemId     INT     NOT NULL,
        itemCount  INT     NOT NULL,
        PRIMARY KEY (accountId, slotIndex)
    );
END
GO
