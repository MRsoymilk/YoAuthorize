use sqlx::postgres::PgPoolOptions;

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    let database_url = authorization_api::config::required_value("DATABASE_URL")?;
    let email = std::env::var("BOOTSTRAP_ADMIN_EMAIL")?
        .trim()
        .to_ascii_lowercase();
    let name = std::env::var("BOOTSTRAP_ADMIN_NAME").unwrap_or_else(|_| "Administrator".into());
    let password = authorization_api::config::required_value("BOOTSTRAP_ADMIN_PASSWORD")?;
    if password.len() < 14 {
        anyhow::bail!("BOOTSTRAP_ADMIN_PASSWORD must be at least 14 characters");
    }
    let hash = authorization_api::security::hash_password(&password)?;
    let pool = PgPoolOptions::new().connect(&database_url).await?;
    if sqlx::query_scalar::<_, bool>("SELECT EXISTS(SELECT 1 FROM users WHERE role='admin')")
        .fetch_one(&pool)
        .await?
    {
        anyhow::bail!("an administrator already exists; bootstrap is one-shot");
    }
    sqlx::query(
        "INSERT INTO users(email,name,password_hash,role,status) VALUES($1,$2,$3,'admin','active')",
    )
    .bind(email)
    .bind(name)
    .bind(hash)
    .execute(&pool)
    .await?;
    println!("bootstrap admin created");
    Ok(())
}
