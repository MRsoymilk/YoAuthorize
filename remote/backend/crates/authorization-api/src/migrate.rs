use authorization_api::config::required_value;
use sqlx::postgres::PgPoolOptions;

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    let database_url = required_value("DATABASE_URL")?;
    let pool = PgPoolOptions::new()
        .max_connections(1)
        .connect(&database_url)
        .await?;
    sqlx::migrate!().run(&pool).await?;
    println!("database migrations completed");
    Ok(())
}
