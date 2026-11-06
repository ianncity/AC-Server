-- Repair prestige Dreadmist reward appearances to match the native WotLK set rows.
UPDATE `item_template` SET `displayid` = CASE `entry`
    WHEN 96141 THEN 31263
    WHEN 96142 THEN 29798
    WHEN 96143 THEN 29792
    WHEN 96144 THEN 29800
    WHEN 96145 THEN 29797
    WHEN 96146 THEN 29799
    WHEN 96147 THEN 29793
    WHEN 96148 THEN 29795
END
WHERE `entry` BETWEEN 96141 AND 96148;