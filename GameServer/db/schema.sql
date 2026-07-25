-- Project-W 게임 서버 스키마.
-- 멱등 스크립트: 반복 실행해도 안전하다.
-- 적용: sqlcmd -S "(localdb)\MSSQLLocalDB" -i db\schema.sql
-- 문자열 컬럼 크기는 protocol.hpp의 kLoginIdMax/kNicknameMax(널 포함)에서 널을 뺀 값과 일치해야 한다.

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

-- 다음 단계(룸서버 인벤토리)용 선반영.
IF OBJECT_ID(N'dbo.Inventory', N'U') IS NULL
CREATE TABLE dbo.Inventory (
    accountId  BIGINT  NOT NULL REFERENCES dbo.Account(accountId),
    itemId     INT     NOT NULL,
    itemCount  INT     NOT NULL DEFAULT 1,
    PRIMARY KEY (accountId, itemId)
);
GO
